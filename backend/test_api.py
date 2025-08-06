import requests
import time

def test_backend():
    base_url = "http://localhost:8000"
    
    print("Testing Backend API...")
    
    # Test health endpoint
    try:
        health_response = requests.get(f"{base_url}/api/health")
        print(f"Health Status: {health_response.json()}")
    except Exception as e:
        print(f"Health check failed: {e}")
        return
    
    # Test AQI endpoint
    try:
        aqi_response = requests.get(f"{base_url}/api/aqi")
        if aqi_response.status_code == 200:
            data = aqi_response.json()
            print(f"AQI Data: {data}")
        else:
            print(f"AQI request failed: {aqi_response.status_code} - {aqi_response.text}")
    except Exception as e:
        print(f"AQI request failed: {e}")

if __name__ == "__main__":
    test_backend() 