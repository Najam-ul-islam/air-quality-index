from fastapi import FastAPI, HTTPException
# from fastapi.responses import JSONResponse
from fastapi.middleware.cors import CORSMiddleware
import joblib
import os
import pandas as pd
import serial
import json
import time
from contextlib import asynccontextmanager
from typing import Optional

# Configuration
MODEL_PATH = "saved_model/aqi_model.pkl"
# SERIAL_PORT = "/dev/ttyUSB0"
SERIAL_PORT = "COM4"
BAUD_RATE = 115200
# REQUIRED_FEATURES = ["PM2.5", "PM10", "NO", "NO2", "NH3", "CO", "SO2", "O3"]
REQUIRED_FEATURES = ["PM2.5", "PM10", "NH3", "CO"]

# Global variables
ser = None
model = None


@asynccontextmanager
async def lifespan(app: FastAPI):
    global ser, model

    # Load ML model
    try:
        if not os.path.exists(MODEL_PATH):
            raise FileNotFoundError(f"Model not found at {MODEL_PATH}")
        model = joblib.load(MODEL_PATH)
        print("✅ ML model loaded successfully")

        # Verify model features
        if list(model.feature_names_in_) != REQUIRED_FEATURES:
            raise ValueError("Model features don't match required features")

    except Exception as e:
        print(f"⚠️ Model loading error: {e}")
        model = None

    # Initialize serial connection
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        time.sleep(3)  # Wait for Arduino reset
        ser.reset_input_buffer()
        print(f"✅ Serial connected on {SERIAL_PORT} @ {BAUD_RATE} baud")
    except Exception as e:
        ser = None
        print(f"⚠️ Serial connection failed: {e}")

    yield

    # Cleanup
    if ser:
        ser.close()
        print("🔌 Serial connection closed")


app = FastAPI(lifespan=lifespan)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


def validate_sensor_data(data: dict) -> bool:
    """Validate sensor values within reasonable ranges"""
    ranges = {
        "PM2.5": (0, 1000),
        "PM10": (0, 1000),
        "CO": (0, 1000),
        "NH3": (0, 500),
        # "NO": (0, 10),
        # "NO2": (0, 10),
        # "SO2": (0, 500),
        # "O3": (0, 500),
        "temp": (-40, 85),  # Optional display params
        "hum": (0, 100),  # Optional display params
    }

    for field, (min_val, max_val) in ranges.items():
        value = data.get(field)
        if value is None and field in REQUIRED_FEATURES:
            return False
        if value is not None and not (min_val <= value <= max_val):
            return False
    return True


def read_arduino_data() -> Optional[dict]:
    """Read and parse sensor data with retries"""
    if not ser:
        return None

    try:
        line = ser.readline().decode("utf-8").strip()
        print(f"📡 Raw data: {line}")

        data = json.loads(line)

        # Convert Arduino's PM25 key to PM2.5
        if "PM25" in data:
            data["PM2.5"] = data.pop("PM25")

        if validate_sensor_data(data):
            return data

    except json.JSONDecodeError:
        print("⚠️ Invalid JSON format")
    except KeyError as e:
        print(f"⚠️ Missing required field: {e}")
    except Exception as e:
        print(f"⚠️ Error reading data: {e}")

    return None


@app.get("/api/health")
async def health_check():
    return {
        "serial_connected": ser is not None,
        "model_loaded": model is not None,
        "required_features": REQUIRED_FEATURES,
    }


@app.get("/api/aqi")
async def get_aqi_prediction():
    sensor_data = read_arduino_data()

    if not sensor_data:
        raise HTTPException(status_code=500, detail="Failed to read sensor data")

    try:
        # Prepare input for ML model (only required features)
        input_data = {feature: sensor_data[feature] for feature in REQUIRED_FEATURES}

        # Create DataFrame with correct column order
        input_df = pd.DataFrame([input_data], columns=REQUIRED_FEATURES)
        # Make prediction
        if model is None:
            raise HTTPException(status_code=500, detail="ML model not loaded")
            
        prediction = model.predict(input_df)[0]
        aqi_status = get_aqi_status(prediction)

        return {
            "aqi": round(prediction, 2),
            "status": aqi_status,
            "sensor_data": {
                **input_data,
                "temp": sensor_data.get("temp"),
                "hum": sensor_data.get("hum"),
            },
        }

    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Prediction error: {str(e)}")


def get_aqi_status(aqi: float) -> str:
    """Convert AQI value to status category"""
    if aqi <= 50:
        return "Good"
    elif aqi <= 100:
        return "Moderate"
    elif aqi <= 150:
        return "Unhealthy for Sensitive Groups"
    elif aqi <= 200:
        return "Unhealthy"
    elif aqi <= 300:
        return "Very Unhealthy"
    else:
        return "Hazardous"


if __name__ == "__main__":
    import uvicorn

    uvicorn.run("main:app", host="0.0.0.0", port=8000, reload=True)





# from fastapi import FastAPI
# import joblib
# import os
# import pandas as pd
# from fastapi.middleware.cors import CORSMiddleware
# import serial
# import json
# import time
# from contextlib import asynccontextmanager

# # Global variable for serial connection
# ser = None

# # Load model
# MODEL_PATH = "saved_model/aqi_model.pkl"
# if not os.path.exists(MODEL_PATH):
#     raise FileNotFoundError(f"Model not found at path: {MODEL_PATH}")

# model = joblib.load(MODEL_PATH)

# # Serial port configuration
# # SERIAL_PORT = '/dev/ttyUSB0'
# SERIAL_PORT = 'COM4'
# BAUD_RATE = 115200

# # Define the features and AQI bucket logic
# FEATURES = ["PM2.5", "PM10", "NO", "NO2", "NH3", "CO", "SO2", "O3"]

# @asynccontextmanager
# async def lifespan(app: FastAPI):
#     global ser
#     # Startup
#     try:
#         ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1.0)
#         time.sleep(3)
#         # print(f"Connected to Arduino on {SERIAL_PORT}")
#         print(f"✅ Serial connection established! on Serial Port {SERIAL_PORT} at Baud Rate: {BAUD_RATE}\n")
#         ser.reset_input_buffer()
#     except Exception as e:
#         print(f"Error connecting to Arduino: {e}")
#         ser = None
#     yield
#     # Shutdown
#     if ser:
#         ser.close()
#         print("Serial connection closed")

# app = FastAPI(lifespan=lifespan)

# # CORS settings for frontend
# app.add_middleware(
#     CORSMiddleware,
#     allow_origins=["*"],  # Allow all for testing
#     allow_credentials=True,
#     allow_methods=["*"],
#     allow_headers=["*"],
# )



# def get_aqi_bucket(aqi: float) -> str:
#     if aqi <= 50:
#         return "Good"
#     elif aqi <= 100:
#         return "Satisfactory"
#     elif aqi <= 200:
#         return "Moderate"
#     elif aqi <= 300:
#         return "Poor"
#     elif aqi <= 400:
#         return "Very Poor"
#     else:
#         return "Severe"

# def convert_arduino_to_json(data_string: str):
#     """
#     Convert Arduino data string to JSON format.
    
#     Args:
#         data_string (str): Raw data string received from Arduino
        
#     Returns:
#         dict: Parsed JSON data or None if conversion fails
#     """
#     try:
#         # Remove any whitespace and newlines
#         clean_data = data_string.strip()
        
#         # Try to parse the JSON data
#         json_data = json.loads(clean_data)
        
#         # Validate the data structure
#         if not isinstance(json_data, dict):
#             print("Received data is not a JSON object")
#             return None
            
#         # Ensure all required fields are present and convert to float
#         required_fields = ["PM25", "PM10", "NO", "NO2", "NH3", "CO", "SO2", "O3"]
#         for field in required_fields:
#             if field not in json_data:
#                 print(f"Missing required field: {field}")
#                 return None
#             try:
#                 json_data[field] = float(json_data[field])
#             except (ValueError, TypeError):
#                 print(f"Invalid value for {field}")
#                 return None
                   
#         return json_data
    
        
#     except json.JSONDecodeError as e:
#         print(f"JSON decode error: {e}")
#         return None
#     except Exception as e:
#         print(f"Error converting data: {e}")
#         return None

# def read_arduino_data():
#     if not ser:
#         return None
#     try:
#         if ser.in_waiting > 0:
#             line = ser.readline().decode('utf-8').strip()
#             return convert_arduino_to_json(line)
#     except serial.SerialException as e:
#         print(f"Error reading from Arduino: {e}")
#         return None
#     return None

# # Serial Test API
# @app.get("/api/test")
# async def test_serial():
#     data = read_arduino_data()
#     return {"data": data}

# @app.get("/api/aqi")
# async def get_aqi():
#     # Read data from Arduino
#     sensor_data = read_arduino_data()
    
#     if not sensor_data:
#         return {"error": "Could not read data from Arduino"}
    
#     try:
#         # Prepare data for prediction
#         input_data = {
#             "PM2.5": float(sensor_data.get("PM25", 0)),
#             "PM10": float(sensor_data.get("PM10", 0)),
#             "NO": float(sensor_data.get("NO", 0)),
#             "NO2": float(sensor_data.get("NO2", 0)),
#             "NH3": float(sensor_data.get("NH3", 0)),
#             "CO": float(sensor_data.get("CO", 0)),
#             "SO2": float(sensor_data.get("SO2", 0)),
#             "O3": float(sensor_data.get("O3", 0))
#         }
        
#         # Create DataFrame for prediction
#         input_df = pd.DataFrame([input_data])
        
#         # Make prediction
#         prediction = model.predict(input_df)[0]
#         status = get_aqi_bucket(prediction)
        
#         return {
#             "aqi": round(prediction, 2),
#             "status": status,
#             "pm25": input_data["PM2.5"],
#             "pm10": input_data["PM10"],
#             "no": input_data["NO"],
#             "no2": input_data["NO2"],
#             "nh3": input_data["NH3"],
#             "co": input_data["CO"],
#             "so2": input_data["SO2"],
#             "o3": input_data["O3"]
#         }
        
#     except Exception as e:
#         return {"error": str(e)}

# if __name__ == "__main__":
#     import uvicorn
#     uvicorn.run("main:app", host="0.0.0.0", port=8000, reload=True)




