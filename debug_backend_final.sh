#!/bin/bash

echo "🔧 Debugging Backend Internal Server Error - Final Solution"
echo "=========================================================="

echo "📝 Backend debugging changes:"
echo "   • Thêm logging chi tiết vào filter timestamp_to_datetime"
echo "   • Thêm try-catch vào dashboard endpoint"
echo "   • Thêm endpoint /debug để test template đơn giản"
echo "   • Template debug đơn giản hơn để test"
echo ""

echo "🔄 CẦN RESTART BACKEND để áp dụng debugging:"
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

echo "🧪 Sau khi restart, test các endpoint:"
echo "   • Dashboard: curl http://cloud.anhnguyxn.io.vn:8000/"
echo "   • Debug endpoint: curl http://cloud.anhnguyxn.io.vn:8000/debug"
echo "   • API readings: curl http://cloud.anhnguyxn.io.vn:8000/api/readings"
echo ""

echo "📊 Kiểm tra logs backend để xem lỗi chi tiết:"
echo "   • Docker logs: docker compose logs -f"
echo "   • Server logs: journalctl -f -u service_name"
echo ""

echo "🔍 Nếu vẫn lỗi, có thể là:"
echo "   • Template Jinja2 có vấn đề với filter"
echo "   • Database connection issue"
echo "   • Python version compatibility"
echo ""

echo "✅ Sau khi restart, sẽ có logging chi tiết để debug!"
