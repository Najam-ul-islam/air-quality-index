# AQI Monitoring System - Troubleshooting Guide

## Issue: Dashboard shows "Loading data..." and doesn't display sensor data

### Step 1: Check Backend Server
1. Make sure the backend server is running:
   ```bash
   cd backend
   python main.py
   ```
   You should see output like:
   ```
   ✅ ML model loaded successfully
   ✅ Serial connected on COM4 @ 115200 baud
   ```

### Step 2: Check Serial Connection
1. Verify Arduino is connected to the correct COM port (currently set to COM4)
2. Check if Arduino is sending data by looking at the backend console output
3. If you see "⚠️ Serial connection failed", check:
   - Arduino is connected to the correct USB port
   - No other application is using the serial port
   - Arduino code is uploaded and running

### Step 3: Test API Endpoints
Run the test script to check if the API is working:
```bash
cd backend
python test_api.py
```

### Step 4: Check Frontend
1. Make sure the frontend is running:
   ```bash
   cd frontend/aqi_dashboard
   npm run dev
   ```
2. Open browser console (F12) and check for any errors
3. The dashboard should show backend status indicators

### Step 5: Common Issues and Solutions

#### Issue: "Backend server not running"
**Solution:** Start the backend server as shown in Step 1

#### Issue: "Serial: ❌ Disconnected"
**Solutions:**
1. Check if Arduino is connected to COM4 (or change the port in `backend/main.py`)
2. Make sure Arduino code is uploaded and running
3. Try a different USB cable or port

#### Issue: "Model: ❌ Not Loaded"
**Solutions:**
1. Check if `saved_model/aqi_model.pkl` exists in the backend folder
2. If missing, run the model training script:
   ```bash
   cd backend
   python model_train.py
   ```

#### Issue: "JSON decoding failed"
**Solutions:**
1. Check if Arduino is sending valid JSON data
2. Verify Arduino code is using `serializeJson(doc, Serial);` without boundary markers
3. Check serial baud rate matches (115200)

#### Issue: "Failed to read sensor data"
**Solutions:**
1. Check if Arduino is sending data in the expected format
2. Verify all required sensor fields are present: PM2.5, PM10, NH3, CO
3. Check sensor connections and wiring

### Step 6: Debug Mode
To see raw data from Arduino, uncomment the print statement in `backend/main.py`:
```python
print(f"📡 Raw data: {line}")
```

### Step 7: Mock Data Testing
If Arduino is not available, the system will use mock data for testing. This allows you to verify the frontend works correctly.

### Step 8: Hardware Checklist
- [ ] Arduino connected to USB
- [ ] All sensors properly wired
- [ ] Arduino code uploaded successfully
- [ ] Sensors are powered and working
- [ ] No loose connections

### Step 9: Software Checklist
- [ ] Backend server running on port 8000
- [ ] Frontend running on port 3000
- [ ] ML model file exists
- [ ] All Python dependencies installed
- [ ] Node.js dependencies installed

## Expected Data Format
The Arduino should send JSON data like this:
```json
{
  "temp": 24.5,
  "hum": 65.0,
  "PM25": 25.0,
  "PM10": 45.0,
  "CO": 1.2,
  "NH3": 2.5,
  "AQI": 75,
  "dht_error": false
}
```

## Contact
If issues persist, check the console output for specific error messages and refer to the error handling in the code. 