#!/bin/bash
# Script để restart backend với firmware endpoints

echo "🔄 Restarting Battery Monitor Backend with Firmware Support"
echo "=========================================================="

# Kiểm tra Docker
if ! command -v docker &> /dev/null; then
    echo "❌ Docker không được cài đặt"
    exit 1
fi

# Kiểm tra container có đang chạy không
if docker ps | grep -q battery-backend; then
    echo "🛑 Stopping existing container..."
    docker stop battery-backend
    docker rm battery-backend
fi

# Build lại image với code mới
echo "🔨 Building new image..."
docker build -t battery-backend .

# Chạy container mới
echo "🚀 Starting new container..."
docker run -d \
    --name battery-backend \
    -p 82:8000 \
    -e BATTERY_API_KEY="battery_monitor_2024_secure_key" \
    battery-backend

# Kiểm tra container có chạy không
sleep 5
if docker ps | grep -q battery-backend; then
    echo "✅ Backend restarted successfully!"
    echo "🌐 URL: http://cloud.anhnguyxn.io.vn:82"
    echo "📚 API Docs: http://cloud.anhnguyxn.io.vn:82/docs"
    echo ""
    echo "🧪 Test firmware endpoints:"
    echo "curl http://cloud.anhnguyxn.io.vn:82/api/firmware/info"
    echo "curl http://cloud.anhnguyxn.io.vn:82/api/firmware/check?current_version=1.0.0-build1"
else
    echo "❌ Failed to start backend"
    echo "📋 Check logs:"
    docker logs battery-backend
    exit 1
fi
