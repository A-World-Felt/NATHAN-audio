# Démo web — pipeline audio HRTF

Vitrine interactive du pipeline audio (profils HRTF CIPIC, décodage MP3, spatialisation
R-AUD-03), pour démonstration et enregistrement vidéo. Design complet :
[`docs/superpowers/specs/2026-08-06-web-audio-demo-design.md`](../../docs/superpowers/specs/2026-08-06-web-audio-demo-design.md).

Le moteur audio réel (`server/`, C++/OpenAL) n'est pas réécrit : la page web (`client/`,
Vite/TypeScript) n'est qu'un client qui affiche ce que le serveur mesure et pilote sa
position/son profil. **Le son sort par la sortie audio de l'OS (OpenAL natif), pas par
l'onglet du navigateur.**

## Lancer la démo

1. Compiler le projet à la racine du repo (voir le `README.md` racine — vcpkg + CMake), puis :

   ```powershell
   .\build\Debug\web_demo_server.exe
   ```

   Démarre le moteur audio (profil HRTF par défaut, 4 sources en boucle) et l'API locale sur
   `http://127.0.0.1:8787`. Laisser tourner.

2. Dans un second terminal :

   ```powershell
   cd examples/web-demo/client
   npm install
   npm run dev
   ```

   Ouvrir l'URL affichée (par défaut `http://localhost:5173`).

## Utilisation

- **Glisser le X** (joueur) à la souris — le son suit en temps réel.
- **Cliquer un profil** dans la liste de gauche — bascule HRTF réelle, audible en quelques
  centaines de millisecondes.
- Le panneau **Sources** et le panneau **Décodage MP3** affichent des valeurs mesurées par le
  serveur C++, pas des approximations calculées dans le navigateur.

## Enregistrement vidéo

Capturer le **son du bureau** (OBS : "Desktop Audio" / capture audio système), pas le son de
l'onglet du navigateur — la sortie audio vient d'OpenAL, hors du navigateur.

## Configuration

`examples/web-demo/client/.env.example` → copier en `.env` pour changer `VITE_SERVER_URL` si
le serveur tourne sur un autre port.
