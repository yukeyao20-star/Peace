#!/usr/bin/env bash
# ============================================================================
#  在 VPS 上安装流量统计（让 Clash 订阅卡片显示"已用 / 总量"）
#
#  用法：
#    ./install.sh              装脚本 + systemd timer
#    ./install.sh --nginx      连 nginx 订阅配置一起推上去并 reload
#
#  依赖：本机能 ssh 到 ~/.ssh/config 里的 vps 别名
#
#  装了什么：
#    · vnstat（如果没装）—— 按接口记账，跨重启保留
#    · libnginx-mod-http-perl（如果没装）—— 让 nginx 每次请求读一次用量文件
#    · /usr/local/bin/vps-traffic          —— 把用量写成订阅头字符串
#    · /etc/systemd/system/vps-traffic.{service,timer}  —— 每分钟跑一次
# ============================================================================
set -euo pipefail

HOST="${HOST:-vps}"
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "==> 检查/安装依赖包"
ssh "${HOST}" 'NEED=""; dpkg -s vnstat >/dev/null 2>&1 || NEED="$NEED vnstat";
               dpkg -s libnginx-mod-http-perl >/dev/null 2>&1 || NEED="$NEED libnginx-mod-http-perl";
               if [ -n "$NEED" ]; then
                   echo "    需要安装：$NEED"
                   DEBIAN_FRONTEND=noninteractive apt-get install -y $NEED
               else
                   echo "    依赖齐全"
               fi'

echo "==> 安装 /usr/local/bin/vps-traffic"
scp -q "${DIR}/vps-traffic" "${HOST}:/usr/local/bin/vps-traffic"
ssh "${HOST}" 'chmod 755 /usr/local/bin/vps-traffic'

echo "==> 安装 systemd 单元"
scp -q "${DIR}/vps-traffic.service" "${HOST}:/etc/systemd/system/vps-traffic.service"
scp -q "${DIR}/vps-traffic.timer"   "${HOST}:/etc/systemd/system/vps-traffic.timer"
ssh "${HOST}" 'systemctl daemon-reload && systemctl enable --now vps-traffic.timer'

echo "==> 立刻跑一次并检查产物"
ssh "${HOST}" 'systemctl start vps-traffic.service; sleep 1; cat /var/lib/vps-traffic/userinfo.txt; echo'

if [ "${1:-}" = "--nginx" ]; then
  echo "==> 更新 nginx 订阅配置"
  TS="$(date +%Y%m%d-%H%M%S)"
  ssh "${HOST}" "mkdir -p /root/nginx-backup-${TS} && cp -a /etc/nginx/snippets /etc/nginx/conf.d /root/nginx-backup-${TS}/"
  scp -q "${DIR}/../nginx/traffic-userinfo.conf" "${HOST}:/etc/nginx/conf.d/traffic-userinfo.conf"
  scp -q "${DIR}/../nginx/homestead-sub.conf"    "${HOST}:/etc/nginx/snippets/homestead-sub.conf"
  ssh "${HOST}" "nginx -t && systemctl reload nginx && echo '>>> nginx 已重载'"
fi

echo
echo "==> 完成。验证（换成你自己的订阅路径）："
echo "    ssh ${HOST} \"curl -s -u 用户名:密码 -I http://127.0.0.1/<订阅路径>/ | grep -i subscription-userinfo\""
