package org.nightjar.re4;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

/**
 * Retour de la connexion Discord (13/10) : discord-1559162514154201128:/authorize/callback?code=...&state=...
 * envoye par l'appli Discord (ecran « Authorize » de l'appli) ou par le navigateur (repli).
 * Transparente : elle remet l'adresse au lanceur natif (RE4Activity.pollDiscordAuth), ramene le jeu
 * devant sans le recreer (RE4Activity est singleTask) et se ferme.
 */
public class AuthCallback extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Uri data = getIntent() != null ? getIntent().getData() : null;
        if (data != null) RE4Activity.deliverDiscordAuth(data.toString());
        Intent back = new Intent(this, RE4Activity.class);
        back.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
        try {
            startActivity(back);
        } catch (Exception ignored) {
        }
        finish();
    }
}
