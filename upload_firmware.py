#!/usr/bin/env python3
"""
Battery Monitor Firmware Upload Tool (Python)
Upload firmware từ máy local lên backend server
"""

import os
import sys
import json
import requests
import argparse
from pathlib import Path

def upload_firmware(firmware_file, version, server_url, build=1, release_notes="Bug fixes", api_key=None):
    """Upload firmware lên backend server"""
    
    # Kiểm tra file firmware
    if not os.path.exists(firmware_file):
        print(f"❌ File firmware không tồn tại: {firmware_file}")
        return False
    
    if not firmware_file.endswith('.bin'):
        print("❌ File phải có extension .bin")
        return False
    
    # Sử dụng API key từ environment hoặc default
    if not api_key:
        api_key = os.getenv('BATTERY_API_KEY', 'battery_monitor_2024_secure_key')
    
    print(f"📁 Firmware file: {firmware_file}")
    print(f"📱 Version: {version}")
    print(f"🔢 Build: {build}")
    print(f"📝 Release notes: {release_notes}")
    print(f"🌐 Server URL: {server_url}")
    print(f"🔑 API Key: {api_key[:8]}...{api_key[-8:]}")
    print("")
    
    # Kiểm tra kết nối server
    print("🔍 Kiểm tra kết nối server...")
    try:
        response = requests.get(f"{server_url}/api/firmware/info", timeout=10)
        if response.status_code == 200:
            print("✅ Server accessible")
        else:
            print(f"⚠️ Server response: {response.status_code}")
    except requests.exceptions.RequestException as e:
        print(f"❌ Không thể kết nối đến server: {e}")
        return False
    
    # Upload firmware
    print("🔄 Đang upload firmware...")
    
    try:
        with open(firmware_file, 'rb') as f:
            files = {'file': (os.path.basename(firmware_file), f, 'application/octet-stream')}
            data = {
                'version': version,
                'build': build,
                'release_notes': release_notes
            }
            headers = {'X-API-Key': api_key}
            
            response = requests.post(
                f"{server_url}/api/firmware/upload",
                files=files,
                data=data,
                headers=headers,
                timeout=60
            )
        
        print(f"📤 Response: {response.text}")
        
        if response.status_code == 200:
            result = response.json()
            if result.get('status') == 'success':
                print("")
                print("✅ Upload thành công!")
                print("📱 ESP32 sẽ tự động phát hiện firmware mới trong lần check tiếp theo")
                print("")
                print("🔍 Test firmware check:")
                print(f"curl \"{server_url}/api/firmware/check?current_version=1.0.0-build1&device_id=battery_monitor_001\"")
                print("")
                print("📊 Firmware info:")
                try:
                    info_response = requests.get(f"{server_url}/api/firmware/info")
                    if info_response.status_code == 200:
                        info = info_response.json()
                        print(json.dumps(info, indent=2))
                except:
                    print("Không thể lấy thông tin firmware")
                return True
            else:
                print(f"❌ Upload thất bại: {result.get('message', 'Unknown error')}")
                return False
        else:
            print(f"❌ Upload thất bại: HTTP {response.status_code}")
            print(f"Response: {response.text}")
            return False
            
    except requests.exceptions.RequestException as e:
        print(f"❌ Lỗi upload: {e}")
        return False
    except Exception as e:
        print(f"❌ Lỗi không xác định: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(description='Upload firmware lên Battery Monitor backend')
    parser.add_argument('firmware_file', help='Đường dẫn file firmware .bin')
    parser.add_argument('version', help='Version firmware (ví dụ: 1.0.1)')
    parser.add_argument('server_url', help='URL backend server')
    parser.add_argument('--build', type=int, default=1, help='Build number (default: 1)')
    parser.add_argument('--release-notes', default='Bug fixes and improvements', help='Release notes')
    parser.add_argument('--api-key', help='API key (hoặc dùng BATTERY_API_KEY env var)')
    
    args = parser.parse_args()
    
    success = upload_firmware(
        args.firmware_file,
        args.version,
        args.server_url,
        args.build,
        args.release_notes,
        args.api_key
    )
    
    sys.exit(0 if success else 1)

if __name__ == '__main__':
    main()
