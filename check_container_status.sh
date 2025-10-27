#!/bin/bash
# Script kiểm tra tình trạng container và logs

echo "🔍 Kiểm tra tình trạng Container"
echo "==============================="

echo "📋 Container status:"
docker ps | grep battery-backend

echo ""
echo "📋 Container logs (last 20 lines):"
docker logs --tail 20 battery-backend

echo ""
echo "📋 Port binding:"
docker port battery-backend

echo ""
echo "📋 Container details:"
docker inspect battery-backend | grep -E "(Status|State|IPAddress|Ports)"

echo ""
echo "🧪 Test local connection:"
curl -s --connect-timeout 5 http://localhost:8000/api/firmware/info || echo "❌ Local connection failed"

echo ""
echo "🧪 Test external connection:"
curl -s --connect-timeout 5 http://cloud.anhnguyxn.io.vn:8000/api/firmware/info || echo "❌ External connection failed"
