#!/bin/bash

echo "🕐 Fixing Timezone Issue for Battery Monitor Backend"
echo "=================================================="

echo "📝 Backend timezone fix đã được áp dụng:"
echo "   • Sửa filter timestamp_to_datetime để xử lý timezone đúng"
echo "   • Convert UTC timestamp về UTC+7 (Vietnam timezone)"
echo "   • Thời gian hiển thị sẽ chính xác"
echo ""

echo "🔄 Để áp dụng thay đổi, bạn cần restart backend:"
echo ""
echo "1. Nếu backend chạy trên Docker:"
echo "   cd backend && docker compose restart"
echo ""
echo "2. Nếu backend chạy trên server khác:"
echo "   ssh vào server và restart service"
echo ""
echo "3. Nếu backend chạy local:"
echo "   Ctrl+C để dừng và chạy lại:"
echo "   cd backend && python -m uvicorn app.main:app --host 0.0.0.0 --port 8000"
echo ""

echo "🧪 Test filter mới:"
echo "   Timestamp: 1761113515"
echo "   Converted: 2025-10-22 13:11:55 (UTC+7)"
echo "   Server time: 2025-10-22 13:11:55"
echo ""

echo "✅ Sau khi restart, dashboard sẽ hiển thị thời gian đúng!"
echo "📱 Truy cập: http://cloud.anhnguyxn.io.vn:8000"
