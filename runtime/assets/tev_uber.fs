#version 330 core

in vec4 fragColor;
in vec2 fragTexCoord[8];

out vec4 finalColor;

uniform sampler2D tex[8];

uniform int num_tev_stages;

// TEV Register state
uniform vec4 tev_color_env[16]; // Color operation registers
uniform vec4 tev_alpha_env[16]; // Alpha operation registers
uniform vec4 tev_order[16];     // Texture and Color channel selections
uniform vec4 tev_kcolor_sel[16];
uniform vec4 tev_kalpha_sel[16];

// TEV hardware registers
vec4 tev_regs[4]; // prev, reg0, reg1, reg2
vec4 konst_colors[4];

void main() {
    tev_regs[0] = vec4(0.0); // prev
    tev_regs[1] = vec4(1.0); // reg0
    tev_regs[2] = vec4(1.0); // reg1
    tev_regs[3] = vec4(1.0); // reg2
    
    konst_colors[0] = vec4(1.0);
    konst_colors[1] = vec4(1.0);
    konst_colors[2] = vec4(1.0);
    konst_colors[3] = vec4(1.0);
    
    vec4 ras_color = fragColor;
    
    for (int i = 0; i < num_tev_stages; i++) {
        // Parse TEV order
        int tex_map = int(tev_order[i].x);
        int tex_coord = int(tev_order[i].y);
        int color_chan = int(tev_order[i].z);
        
        vec4 tex_color = vec4(1.0);
        if (tex_map != 255) {
            // For now, simplify and just sample the first texture if any map is used
            // In a real uber shader, we'd do a switch(tex_map) to sample the right texture using the right tex_coord
            if (tex_map == 0) tex_color = texture(tex[0], fragTexCoord[0]);
            else if (tex_map == 1) tex_color = texture(tex[1], fragTexCoord[1]);
            else if (tex_map == 2) tex_color = texture(tex[2], fragTexCoord[2]);
            else if (tex_map == 3) tex_color = texture(tex[3], fragTexCoord[3]);
        }
        
        // Very simplified combiners for this prototype
        // A full emulator would extract A, B, C, D from tev_color_env and tev_alpha_env, and do:
        // out = (d + ((a - b) * c) >> bias) * scale + clamp
        
        // For our prototype, if we're running TEV stage 0, just output texture * color
        if (tex_map != 255) {
            tev_regs[0] = tex_color * ras_color;
        } else {
            tev_regs[0] = ras_color;
        }
    }
    
    finalColor = tev_regs[0];
}
