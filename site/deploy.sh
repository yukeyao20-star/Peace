#!/usr/bin/env bash
# ============================================================
#  湖畔家园官网 —— 部署脚本
#
#  用法：
#    ./deploy.sh              只同步网站文件到 VPS
#    ./deploy.sh --nginx      顺带更新 nginx 配置并 reload
#
#  依赖：本机能 ssh 到 ~/.ssh/config 里的 vps 别名
#  说明：nginx 配置改动会先 nginx -t 校验，失败则不动线上
# ============================================================
set -euo pipefail

HOST="${HOST:-vps}"
REMOTE="${REMOTE:-/var/www/game}"
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "==> 同步网页文件  $DIR  ->  $HOST:$REMOTE"
ssh "$HOST" "mkdir -p $REMOTE/download"
rsync -az --delete \
  --exclude 'download/' --exclude 'nginx/' --exclude 'vps-traffic/' \
  --exclude 'deploy.sh' --exclude '.DS_Store' \
  "$DIR/" "$HOST:$REMOTE/"

echo "==> 同步安装包  $DIR/download/  ->  $HOST:$REMOTE/download/"
rsync -az "$DIR/download/" "$HOST:$REMOTE/download/"

ssh "$HOST" "chown -R www-data:www-data $REMOTE"

if [ "${1:-}" = "--nginx" ]; then
  echo "==> 更新 nginx 配置"
  TS="$(date +%Y%m%d-%H%M%S)"
  ssh "$HOST" "mkdir -p /root/nginx-backup-$TS && cp -a /etc/nginx/sites-available /etc/nginx/sites-enabled /etc/nginx/snippets /etc/nginx/conf.d /root/nginx-backup-$TS/"
  scp -q "$DIR/nginx/homestead.conf"         "$HOST:/etc/nginx/sites-available/homestead"
  scp -q "$DIR/nginx/homestead-sub.conf"     "$HOST:/etc/nginx/snippets/homestead-sub.conf"
  scp -q "$DIR/nginx/traffic-userinfo.conf"  "$HOST:/etc/nginx/conf.d/traffic-userinfo.conf"
  ssh "$HOST" "ln -sfn /etc/nginx/sites-available/homestead /etc/nginx/sites-enabled/homestead
               rm -f /etc/nginx/sites-enabled/sub
               nginx -t && systemctl reload nginx && echo '>>> nginx 已重载'"
fi

echo
echo "==> 完成： http://107.173.70.200/"
