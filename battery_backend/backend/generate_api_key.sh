#!/bin/bash
# Script để generate API key cho Battery Monitor

echo "🔑 Battery Monitor API Key Generator"
echo "=================================="

# Generate secure API key
API_KEY=$(openssl rand -hex 32)

echo "Generated API Key: $API_KEY"
echo ""
echo "📋 Setup Instructions:"
echo "====================="
echo ""
echo "1. Set environment variable:"
echo "   export BATTERY_API_KEY=\"$API_KEY\""
echo ""
echo "2. Or create .env file:"
echo "   echo \"BATTERY_API_KEY=$API_KEY\" > .env"
echo ""
echo "3. Update ESP32 config.h:"
echo "   #define APPLICATION_KEY \"$API_KEY\""
echo ""
echo "4. Restart backend:"
echo "   sudo docker-compose down && sudo docker-compose up -d"
echo ""
echo "⚠️  Keep this key secure and don't share it!"
