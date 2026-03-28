# IP Joypi : 10.91.241.75
# IP RPI : 10.91.241.138

#!/usr/bin/env bash
# deploy_joypi.sh — Déploie To-JoyPI/ vers le JoyPi via scp/ssh
# Usage : ./deploy_joypi.sh [ip] [user]
#   Défaut : ip=192.168.1.21  user=pi  mdp=raspberry

set -e

JOYPI_IP="${1:-10.91.241.138}"
JOYPI_USER="${2:-juliann}"
JOYPI_PASS="1234"
REMOTE_DIR="/home/${JOYPI_USER}/Desktop/To-RPI"
LOCAL_DIR="$(cd "$(dirname "$0")/To-VM" && pwd)"

if ! command -v sshpass &>/dev/null; then
    echo "[!] sshpass non trouvé — install : sudo apt install sshpass"
    exit 1
fi

echo "[deploy] Cible : ${JOYPI_USER}@${JOYPI_IP}:${REMOTE_DIR}"
echo "[deploy] Source : ${LOCAL_DIR}"

# Créer le répertoire cible si besoin
sshpass -p "${JOYPI_PASS}" ssh -o StrictHostKeyChecking=no \
    "${JOYPI_USER}@${JOYPI_IP}" "mkdir -p ${REMOTE_DIR}"

# Copier tout To-JoyPI/ vers ~/rocket/
sshpass -p "${JOYPI_PASS}" scp -o StrictHostKeyChecking=no -r \
    "${LOCAL_DIR}/"* "${JOYPI_USER}@${JOYPI_IP}:${REMOTE_DIR}/"

# Rendre les binaires et scripts exécutables
sshpass -p "${JOYPI_PASS}" ssh -o StrictHostKeyChecking=no \
    "${JOYPI_USER}@${JOYPI_IP}" \
    "chmod +x ${REMOTE_DIR}/bin-util/* ${REMOTE_DIR}/bin-proto/* ${REMOTE_DIR}/*.sh 2>/dev/null; echo '[deploy] OK'"

echo "[deploy] Fichiers déployés :"
sshpass -p "${JOYPI_PASS}" ssh -o StrictHostKeyChecking=no \
    "${JOYPI_USER}@${JOYPI_IP}" "find ${REMOTE_DIR} -type f | sort"
