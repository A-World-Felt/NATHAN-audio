#!/bin/bash
# Demarre la capture audio numerique (loopback ALSA) pour filmer un audit de
# house_benchmark en parallele de htop. A executer sur le MPU (Raspberry Pi),
# PAS en local. Voir stop_audit.sh pour tout remettre en etat a la fin.
#
# Usage : ./start_audit.sh [fichier_sortie.wav]

set -e

OUT_FILE="${1:-audit_house_benchmark.wav}"
ASOUNDRC="$HOME/.asoundrc"
ASOUNDRC_BACKUP="$HOME/.asoundrc.audit_backup"
PID_FILE="/tmp/audit_arecord.pid"

if [ -f "$PID_FILE" ]; then
    echo "Un enregistrement d'audit semble deja en cours (PID $(cat "$PID_FILE"))." >&2
    echo "Lance stop_audit.sh d'abord si ce n'est pas le cas." >&2
    exit 1
fi

echo "== 1. Chargement du module loopback ALSA =="
sudo modprobe snd-aloop

if ! aplay -l | grep -qi loopback; then
    echo "Erreur : la carte Loopback n'apparait pas dans 'aplay -l'." >&2
    exit 1
fi

echo "== 2. Redirection de la sortie ALSA par defaut vers le loopback =="
if [ -f "$ASOUNDRC" ] && [ ! -f "$ASOUNDRC_BACKUP" ]; then
    mv "$ASOUNDRC" "$ASOUNDRC_BACKUP"
    echo "~/.asoundrc existant sauvegarde dans $ASOUNDRC_BACKUP"
fi

cat > "$ASOUNDRC" <<'EOF'
pcm.!default {
    type plug
    slave.pcm "hw:Loopback,0,0"
}
ctl.!default {
    type hw
    card Loopback
}
EOF

echo "== 3. Demarrage de l'enregistrement vers $OUT_FILE =="
arecord -D hw:Loopback,1,0 -f S16_LE -r 48000 -c 2 "$OUT_FILE" &
echo $! > "$PID_FILE"

echo ""
echo "Enregistrement audio demarre (PID $(cat "$PID_FILE")) -> $OUT_FILE"
echo "Tu peux maintenant lancer house_benchmark et htop dans d'autres terminaux."
echo "Quand tu as fini, execute ./stop_audit.sh pour arreter et tout restaurer."
