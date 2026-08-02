#!/bin/bash
# Arrete la capture audio demarree par start_audit.sh et restaure la
# configuration ALSA d'origine (sortie sur les Headphones/HDMI physiques).
# A executer sur le MPU (Raspberry Pi).
#
# Usage : ./stop_audit.sh

ASOUNDRC="$HOME/.asoundrc"
ASOUNDRC_BACKUP="$HOME/.asoundrc.audit_backup"
PID_FILE="/tmp/audit_arecord.pid"

echo "== 1. Arret de l'enregistrement =="
if [ -f "$PID_FILE" ]; then
    PID="$(cat "$PID_FILE")"
    if kill -0 "$PID" 2>/dev/null; then
        kill -INT "$PID"
        wait "$PID" 2>/dev/null
        echo "Enregistrement arrete (PID $PID)."
    else
        echo "Aucun processus actif pour le PID $PID (deja arrete ?)."
    fi
    rm -f "$PID_FILE"
else
    echo "Aucun fichier PID trouve, l'enregistrement n'a peut-etre pas ete demarre via start_audit.sh." >&2
fi

echo "== 2. Restauration de la configuration ALSA =="
if [ -f "$ASOUNDRC_BACKUP" ]; then
    mv "$ASOUNDRC_BACKUP" "$ASOUNDRC"
    echo "~/.asoundrc d'origine restaure."
else
    rm -f "$ASOUNDRC"
    echo "~/.asoundrc supprime (aucune sauvegarde trouvee, il n'existait pas avant l'audit)."
fi

echo ""
echo "Configuration audio restauree. Le son repasse par la sortie physique par defaut."
