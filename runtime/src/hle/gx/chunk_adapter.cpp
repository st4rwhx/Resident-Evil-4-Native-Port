// Moteur natif : traduction des images du noyau en commandes de notre rendu (chunk_adapter.h).
#include "runtime/gx/chunk_adapter.h"

#include <algorithm>
#include <cstring>

#include "runtime/cpu_context.h"
#include "runtime/gx_state.h"
#include "runtime/texture_cache_gl.h"

namespace nj::runtime {
extern MMU *g_guest_mem;
}

namespace nj::runtime::gx::chunk {
namespace {

enum Op : uint32_t { kBp = 1, kXf = 2, kDraw = 3, kTexture = 4, kPalette = 5, kForget = 6, kBpState = 7,
                     kGeometry = 8, kDrawGeometry = 9, kForgetGeometry = 10 };
enum Layout : uint32_t { kNormal = 1u << 0, kBinormalTangent = 1u << 1, kColor0 = 1u << 2, kColor1 = 1u << 3,
                         kTex0 = 1u << 4, kMatrix = 1u << 12 };
constexpr uint32_t kTextureCopy = 1;
constexpr size_t kTextureBudget = 160u << 20;
constexpr uint32_t kPaletteArea = 0x17C0000, kPaletteSize = 0x40000;   // zone de passage dans la memoire invitee

// Registres BP qui declenchent quelque chose (copie, jetons, palettes, invalidation, masque) : jamais
// rejoues depuis l'etat complet du debut d'image, qui est de l'etat seulement.
bool bp_has_effect(uint32_t r) {
  return r == 0x45 || r == 0x47 || r == 0x48 || r == 0x52 || r == 0x64 || r == 0x65 || r == 0x66 || r == 0x67 ||
         r == 0xFE;
}

inline float as_float(uint32_t v) {
  float f;
  std::memcpy(&f, &v, 4);
  return f;
}

inline void mul_pos(const float *m, float *v) {
  const float x = v[0], y = v[1], z = v[2];
  v[0] = m[0] * x + m[1] * y + m[2] * z + m[3];
  v[1] = m[4] * x + m[5] * y + m[6] * z + m[7];
  v[2] = m[8] * x + m[9] * y + m[10] * z + m[11];
}
inline void mul_nrm(const float *n, float *v) {
  const float a = v[0], b = v[1], c = v[2];
  v[0] = n[0] * a + n[1] * b + n[2] * c;
  v[1] = n[3] * a + n[4] * b + n[5] * c;
  v[2] = n[6] * a + n[7] * b + n[8] * c;
}

}  // namespace

bool read_file(const std::string &path, Image &out) {
  FILE *f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  char magic[8];
  uint32_t head[14];
  bool ok = std::fread(magic, 8, 1, f) == 1 && std::memcmp(magic, "RE4GXCH1", 8) == 0 &&
            std::fread(head, sizeof head, 1, f) == 1;
  if (ok) {
    out.flags = head[2];
    out.sequence = head[1];
    out.xfb_address = head[7];
    out.xfb_width = head[8];
    out.xfb_height = head[9];
    out.ops.resize(head[3]);
    out.vertices.resize(head[4]);
    out.indices.resize(head[5]);
    out.blob.resize(head[6]);
    ok = (out.ops.empty() || std::fread(out.ops.data(), 4, out.ops.size(), f) == out.ops.size()) &&
         (out.vertices.empty() || std::fread(out.vertices.data(), 1, out.vertices.size(), f) == out.vertices.size()) &&
         (out.indices.empty() || std::fread(out.indices.data(), 2, out.indices.size(), f) == out.indices.size()) &&
         (out.blob.empty() || std::fread(out.blob.data(), 1, out.blob.size(), f) == out.blob.size());
  }
  std::fclose(f);
  return ok;
}

void Adapter::translate(const View &img, std::vector<GxOp> &out) {
  MMU *mmu = g_guest_mem;
  ++frame_no_;
  if (frame_no_ % 30 == 0 || cached_vertices_ > 1500000) {   // oublier les dessins qui ne reviennent plus
    for (auto it = draws_.begin(); it != draws_.end();) {
      if (it->second.last_frame + 90 < frame_no_ || cached_vertices_ > 1500000) {
        cached_vertices_ -= std::min(cached_vertices_, it->second.vertices->size());
        it = draws_.erase(it);
      } else {
        ++it;
      }
    }
  }
  const uint32_t *ops = img.ops;
  const size_t total = img.op_words;
  for (size_t at = 0; at < total;) {
    const uint32_t head = ops[at];
    const uint32_t op = head & 0xFF, n = head >> 8;
    const uint32_t *p = ops + at + 1;
    if (at + 1 + n > total) break;
    at += 1 + n;

    switch (op) {
    case kBp: {
      if (n < 1) break;
      GxOp c;
      c.type = GxOpKind::BPRegister;
      c.reg = (uint16_t)(p[0] >> 24);
      c.val = p[0] & 0xFFFFFF;
      out.push_back(std::move(c));
      break;
    }
    case kBpState: {
      for (uint32_t r = 0; r < n && r < 256; ++r) {
        if (bp_has_effect(r)) continue;
        GxOp c;
        c.type = GxOpKind::BPRegister;
        c.reg = (uint16_t)r;
        c.val = p[r] & 0xFFFFFF;
        out.push_back(std::move(c));
      }
      break;
    }
    case kXf: {
      if (n < 2) break;
      const uint32_t addr = p[0], words = n - 1;
      GxOp c;
      c.type = GxOpKind::XFRegister;
      c.reg = (uint16_t)addr;
      c.payload.resize(words);
      for (uint32_t i = 0; i < words; ++i) {
        const float f = as_float(p[1 + i]);
        c.payload[i] = f;
        const uint32_t a = addr + i;
        if (a < 256) {
          xf_pos_[a] = f;
          if (f != 0.0f) pos_loaded_ = true;
        } else if (a >= 0x400 && a < 0x460) {
          xf_nrm_[a - 0x400] = f;
        }
      }
      out.push_back(std::move(c));
      break;
    }
    case kTexture: {
      if (n < 12 || !mmu) break;
      const uint32_t flags = p[3], address = p[4] & 0x03FFFFFFu, offset = p[10], bytes = p[11];
      const uint64_t key = (uint64_t)p[2] << 32 | p[1];
      if (flags & kTextureCopy) break;   // copie de l'EFB : notre rendu la tient lui-meme
      if (bytes) {
        if (offset + (size_t)bytes > img.blob_bytes) break;
        auto &store = textures_[key];
        if (store.empty()) store.assign(img.blob + offset, img.blob + offset + bytes);
      }
      auto it = textures_.find(key);
      if (it == textures_.end()) break;
      auto where = at_address_.find(address);
      if (where != at_address_.end() && where->second == key) break;   // deja la
      uint8_t *dst = mmu->get_ptr(0x80000000u | address);
      if (!dst) break;
      std::memcpy(dst, it->second.data(), it->second.size());
      hle::TexCache::get().guest_texels_written(address, (uint32_t)it->second.size());
      // ce que cet ecrit recouvre n'est plus ce qu'on croyait
      for (auto e = at_address_.begin(); e != at_address_.end();) {
        const auto sz = textures_.find(e->second);
        const uint32_t len = sz == textures_.end() ? 0 : (uint32_t)sz->second.size();
        if (e->first != address && e->first < address + it->second.size() && address < e->first + len)
          e = at_address_.erase(e);
        else
          ++e;
      }
      at_address_[address] = key;
      break;
    }
    case kPalette: {
      if (n < 3 || !mmu) break;
      const uint32_t tmem = p[0], bytes = p[1], offset = p[2];
      if (offset + (size_t)bytes > img.blob_bytes || bytes == 0 || bytes > kPaletteSize) break;
      if (palette_cursor_ + bytes > kPaletteSize) palette_cursor_ = 0;
      const uint32_t phys = kPaletteArea + palette_cursor_;
      palette_cursor_ = (palette_cursor_ + bytes + 31) & ~31u;
      uint8_t *dst = mmu->get_ptr(0x80000000u | phys);
      if (!dst) break;
      std::memcpy(dst, img.blob + offset, bytes);
      hle::TexCache::get().guest_texels_written(phys, bytes);
      GxOp a, b;
      a.type = b.type = GxOpKind::BPRegister;
      a.reg = 0x64;
      a.val = (phys >> 5) & 0xFFFFFF;
      b.reg = 0x65;
      b.val = ((tmem >> 9) & 0x3FF) | ((bytes >> 5) & 0x7FF) << 10;
      out.push_back(std::move(a));
      out.push_back(std::move(b));
      break;
    }
    case kForget: {
      if (n < 2) break;
      const uint64_t key = (uint64_t)p[1] << 32 | p[0];
      textures_.erase(key);
      for (auto e = at_address_.begin(); e != at_address_.end();) e = e->second == key ? at_address_.erase(e) : std::next(e);
      break;
    }
    case kDraw:
      translate_draw(p, img.vertices, img.vertex_bytes, img.indices, img.index_count, 0, 0, out);
      break;
    case kGeometry: {   // le noyau confie une geometrie de liste : sommets et indices de cette image
      if (n < 6) break;
      const uint64_t key = (uint64_t)p[1] << 32 | p[0];
      const uint32_t vo = p[2], vb = p[3], io = p[4], ic = p[5];
      if ((size_t)vo + vb > img.vertex_bytes || (size_t)io + ic > img.index_count) break;
      Geometry &g = geometry_[key];
      g.vertices.assign(img.vertices + vo, img.vertices + vo + vb);
      g.indices.assign(img.indices + io, img.indices + io + ic);
      g.generation = ++geometry_generation_;
      break;
    }
    case kDrawGeometry: {   // dessin lu dans une geometrie gardee
      if (n < 12) break;
      const uint64_t key = (uint64_t)p[1] << 32 | p[0];
      auto it = geometry_.find(key);
      if (it == geometry_.end()) break;   // ne doit pas arriver : le noyau envoie avant de designer
      const Geometry &g = it->second;
      ++geometry_draws;
      translate_draw(p + 2, g.vertices.data(), g.vertices.size(), g.indices.data(), g.indices.size(), key, g.generation, out);
      break;
    }
    case kForgetGeometry: {
      if (n < 2) break;
      geometry_.erase((uint64_t)p[1] << 32 | p[0]);
      break;
    }
    default:
      break;
    }
  }
}

void Adapter::translate_draw(const uint32_t *p, const uint8_t *vb_all, size_t vb_bytes, const uint16_t *ix_all,
                             size_t ix_count, uint64_t ref, uint32_t ref_generation, std::vector<GxOp> &out) {
  do {

      const uint32_t primitive = p[0], layout = p[1], stride = p[2], vtx_off = p[3], vtx_n = p[4], idx_off = p[5],
                     idx_n = p[6];
      if ((size_t)vtx_off + (size_t)vtx_n * stride > vb_bytes || (size_t)idx_off + idx_n > ix_count ||
          idx_n == 0)
        break;
      const uint8_t *vb = vb_all + vtx_off;
      const uint16_t *ix = ix_all + idx_off;
      // disposition d'un sommet
      uint32_t off = 12, o_nrm = 0, o_c0 = 0, o_t0 = 0, o_mtx = 0;
      if (layout & kNormal) {
        o_nrm = off;
        off += 12;
        if (layout & kBinormalTangent) off += 24;
      }
      if (layout & kColor0) { o_c0 = off; off += 4; }
      if (layout & kColor1) off += 4;
      for (int t = 0; t < 8; ++t) {
        if (!(layout & (kTex0 << t))) continue;
        if (t == 0) o_t0 = off;
        off += 8;
      }
      if (layout & kMatrix) o_mtx = off;
      // matrices par defaut du dessin
      uint8_t def_pos = (uint8_t)(p[7] & 0xFF), def_tex[8];
      def_tex[0] = (uint8_t)(p[7] >> 8); def_tex[1] = (uint8_t)(p[7] >> 16); def_tex[2] = (uint8_t)(p[7] >> 24);
      for (int k = 0; k < 4; ++k) def_tex[3 + k] = (uint8_t)(p[8] >> (8 * k));
      def_tex[7] = (uint8_t)(p[9] & 0xFF);
      // une matrice par sommet (peau) ou une seule pour tout le dessin
      bool per_vertex = false;
      if (layout & kMatrix)
        for (uint32_t v = 0; v < vtx_n && !per_vertex; ++v) per_vertex = vb[v * stride + o_mtx] != def_pos;
      const bool has_pos_matrix = std::any_of(xf_pos_ + (size_t)def_pos * 4, xf_pos_ + std::min<size_t>(256, (size_t)def_pos * 4 + 12),
                                              [](float f) { return f != 0.0f; });
      const uint8_t cls = (uint8_t)(primitive & 0xFF);

      GxOp c;
      c.type = GxOpKind::DrawPrimitive;
      c.prim_type = cls == 1 ? 0xA8 : cls == 2 ? 0xB8 : 0x90;
      const bool single = !per_vertex && has_pos_matrix && (size_t)def_pos * 4 + 12 <= 256;
      if (single) {
        c.xf_gpu = true;
        c.xf_from_replay = true;
        std::memcpy(c.xf_pos, xf_pos_ + (size_t)def_pos * 4, sizeof c.xf_pos);
        const uint32_t nb = 3u * (def_pos & 31);
        if (nb + 9 <= 96) std::memcpy(c.xf_nrm, xf_nrm_ + nb, sizeof c.xf_nrm);
      }
      // Dessin deja vu (memes octets, meme disposition, memes matrices par defaut) : on reprend ses sommets.
      uint64_t key = 0;
      if (!per_vertex) {
        uint64_t h = 0x9E3779B97F4A7C15ull ^ ((uint64_t)primitive << 40) ^ ((uint64_t)layout << 20) ^ idx_n;
        auto mix = [&h](uint64_t w) {
          h ^= w + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
          h *= 0xFF51AFD7ED558CCDull;
        };
        mix(stride); mix(vtx_n); mix(p[7]); mix(p[8]); mix(p[9]); mix(single ? 1 : 0);
        if (ref) {   // geometrie gardee : sa cle, son envoi et la place du dessin disent tout (rien a relire)
          mix(ref); mix(ref_generation); mix(vtx_off); mix(idx_off); mix(0x47454F);
        } else {
          const size_t vbytes = (size_t)vtx_n * stride;
          size_t k = 0;
          for (; k + 8 <= vbytes; k += 8) { uint64_t w; std::memcpy(&w, vb + k, 8); mix(w); }
          for (; k < vbytes; ++k) mix(vb[k]);
          size_t q = 0;
          for (; q + 4 <= idx_n; q += 4) { uint64_t w; std::memcpy(&w, ix + q, 8); mix(w); }
          for (; q < idx_n; ++q) mix(ix[q]);
        }
        key = h | 1;
        auto hit = draws_.find(key);
        if (hit != draws_.end() && hit->second.vertices->size() == idx_n) {
          hit->second.last_frame = frame_no_;
          c.shared_vertices = hit->second.vertices;
          if (single) c.geom_hash = key;
          ++cache_hits;
          out.push_back(std::move(c));
          break;
        }
        ++cache_misses;
      }
      c.vertices.resize(idx_n);
      for (uint32_t i = 0; i < idx_n; ++i) {
        const uint32_t vi = ix[i];
        if (vi >= vtx_n) { c.vertices.resize(i); break; }
        const uint8_t *s = vb + (size_t)vi * stride;
        GxVertex &v = c.vertices[i];
        std::memcpy(v.pos, s, 12);
        v.has_pos = true;
        if (layout & kNormal) {
          std::memcpy(v.norm, s + o_nrm, 12);
          v.has_nrm = true;
        } else {
          v.norm[0] = v.norm[1] = v.norm[2] = 0.0f;
          v.has_nrm = false;
        }
        if (layout & kColor0) {
          for (int k = 0; k < 4; ++k) v.color[k] = s[o_c0 + k] * (1.0f / 255.0f);
          v.has_color = true;
        } else {
          v.color[0] = v.color[1] = v.color[2] = v.color[3] = 1.0f;
          v.has_color = false;
        }
        if (layout & kTex0) {
          std::memcpy(v.tex[0], s + o_t0, 8);
          v.has_tex[0] = true;
        } else {
          v.tex[0][0] = v.tex[0][1] = 0.0f;
          v.has_tex[0] = false;
        }
        v.posMtxIdx = (layout & kMatrix) ? s[o_mtx] : def_pos;
        for (int t = 0; t < 8; ++t) v.texMtxIdx[t] = (layout & kMatrix) ? s[o_mtx + 1 + t] : def_tex[t];
        std::memcpy(v.obj_pos, v.pos, sizeof v.obj_pos);
        std::memcpy(v.obj_norm, v.norm, sizeof v.obj_norm);
        v.pretransformed = false;
        if (per_vertex) {
          const uint32_t pb = (uint32_t)v.posMtxIdx * 4, nb = 3u * (v.posMtxIdx & 31);
          if (pb + 12 <= 256 && nb + 9 <= 96) {
            mul_pos(xf_pos_ + pb, v.pos);
            if (v.has_nrm) mul_nrm(xf_nrm_ + nb, v.norm);
            v.pretransformed = true;
          }
        } else if (single) {
          v.pretransformed = true;
        }
      }
      if (!c.vertices.empty()) {
        if (key && c.vertices.size() == idx_n) {   // cache (hors dessins de peau : dependants des matrices)
          auto shared = std::make_shared<const std::vector<GxVertex>>(std::move(c.vertices));
          c.vertices.clear();
          c.shared_vertices = shared;
          if (single) c.geom_hash = key;
          cached_vertices_ += shared->size();
          draws_[key] = Cached{shared, frame_no_};
        }
        out.push_back(std::move(c));
      }
        } while (false);
}

}  // namespace nj::runtime::gx::chunk
