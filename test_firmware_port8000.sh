#!/bin/bash
# Script test firmware endpoints trên port 8000

echo "🧪 Testing Firmware Endpoints on Port 8000"
echo "=========================================="

BASE_URL="http://cloud.anhnguyxn.io.vn:8000"
API_KEY="battery_monitor_2024_secure_key"

echo "🔍 Testing firmware info endpoint..."
response=$(curl -s "$BASE_URL/api/firmware/info")
echo "Response: $response"
echo ""

echo "🔍 Testing firmware check endpoint..."
response=$(curl -s "$BASE_URL/api/firmware/check?current_version=1.0.0-build1&device_id=battery_monitor_001")
echo "Response: $response"
echo ""

echo "🔍 Testing firmware upload endpoint..."
echo "fake firmware content for testing" > test_firmware.bin
response=$(curl -s -X POST "$BASE_URL/api/firmware/upload" \
    -H "X-API-Key: $API_KEY" \
    -F "file=@test_firmware.bin" \
    -F "version=1.0.1" \
    -F "build=1" \
    -F "release_notes=Test firmware")
echo "Response: $response"
echo ""

echo "🔍 Testing firmware info after upload..."
response=$(curl -s "$BASE_URL/api/firmware/info")
echo "Response: $response"
echo ""

echo "🔍 Testing firmware check after upload..."
response=$(curl -s "$BASE_URL/api/firmware/check?current_version=1.0.0-build1&device_id=battery_monitor_001")
echo "Response: $response"
echo ""

# Cleanup
rm -f test_firmware.bin

echo "✅ Test completed!"
