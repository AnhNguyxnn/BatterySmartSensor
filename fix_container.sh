#!/bin/bash
# Script fix container và restart

echo "🔧 Fix Container và Restart"
echo "=========================="

echo "🛑 Stopping existing container..."
docker stop battery-backend 2>/dev/null || echo "Container not running"
docker rm battery-backend 2>/dev/null || echo "Container not found"

echo ""
echo "🔨 Building new image with python-multipart..."
docker build -t battery-backend .

echo ""
echo "🚀 Starting new container..."
docker run -d \
    --name battery-backend \
    -p 8000:8000 \
    -e BATTERY_API_KEY="battery_monitor_2024_secure_key" \
    battery-backend

echo ""
echo "⏳ Waiting for container to start..."
sleep 5

echo ""
echo "🔍 Checking container status..."
docker ps | grep battery-backend

echo ""
echo "📋 Container logs (last 10 lines):"
docker logs --tail 10 battery-backend

echo ""
echo "🧪 Test firmware endpoints..."
curl -s http://localhost:8000/api/firmware/info || echo "❌ Firmware info failed"
echo ""

curl -s "http://localhost:8000/api/firmware/check?current_version=1.0.0-build1&device_id=battery_monitor_001" || echo "❌ Firmware check failed"
echo ""

echo "✅ Container fix completed!"
echo ""
echo "🔍 Test external connection:"
curl -s --connect-timeout 5 http://cloud.anhnguyxn.io.vn:8000/api/firmware/info || echo "❌ External connection failed"
