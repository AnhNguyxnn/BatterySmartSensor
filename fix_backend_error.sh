#!/bin/bash

echo "🔧 Fixing Backend Internal Server Error"
echo "======================================"

echo "📝 Backend fixes đã được áp dụng:"
echo "   • Sửa filter timestamp_to_datetime với error handling tốt hơn"
echo "   • Hiển thị timestamp có vấn đề thay vì crash"
echo "   • Xử lý timestamp không hợp lệ an toàn"
echo ""

echo "🔄 Cần restart backend để áp dụng thay đổi:"
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
echo "   Timestamp: 1761114127"
echo "   Result: 2025-10-22 13:22:07"
echo ""

echo "✅ Sau khi restart, dashboard sẽ hoạt động bình thường!"
echo "📱 Truy cập: http://cloud.anhnguyxn.io.vn:8000"
