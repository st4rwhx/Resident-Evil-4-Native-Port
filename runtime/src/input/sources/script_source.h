#pragma once
#include "input/input_source.h"

#include <chrono>
#include <cstdint>
#include <vector>

namespace nj::runtime::input {

// Manette scriptee pour les parties automatisees (essais, recensement de l'interpreteur).
//
// NJ_KEYSCRIPT="t:TOUCHES[*ms][,t:TOUCHES[*ms]...]" : a t secondes apres l'arrivee en
// scene (meme critere qu'AutoStartSource : plus de 3000 tirages par image), les TOUCHES sont
// tenues ms millisecondes (150 par defaut). Lettres : A B X Y Z S(tart) L R, U D < > pour la
// croix, w s a d pour le stick (avant, arriere, gauche, droite). Exemple :
//   "5:Y,8:>,9:B,12:Z,15:B,20:w*2000,23:S,26:S"
// ouvre la mallette, y bouge, la ferme, ouvre la carte, marche deux secondes, met en pause.
// P demande une capture de l'image (fichier <NJ_SCREENSHOT>_t<dixiemes de s>.bmp) : c'est ce
// qu'utilise la batterie de non-regression (tools/re4/regress.py).
class ScriptSource : public PadSource {
public:
    ScriptSource();
    ~ScriptSource() override = default;

    void poll(PadState pads[4]) override;

    static bool enabled();

private:
    struct Step {
        double at_s;
        double hold_ms;
        std::uint16_t buttons;
        std::int8_t stick_x, stick_y;
        bool fired;
        bool shot;   // 'P' : capture de l'EFB (main.cpp, NJ_SCREENSHOT)
        bool framelog = false;   // 'F' : journal de chaque dessin de l'image suivante (comme F12)
    };
    std::vector<Step> steps_;
    bool started_ = false;
    std::chrono::steady_clock::time_point origin_;
    uint64_t last_frames_ = 0, last_draws_ = 0;
};

}
