# save_model.py
import pandas as pd
from sklearn.ensemble import RandomForestRegressor
from sklearn.model_selection import train_test_split
import joblib
import os

df = pd.read_csv("city_day.csv")

features = ["PM2.5", "PM10", "NH3", "CO"]
df = df[features + ["AQI"]].dropna()

X = df[features] # Features
y = df["AQI"]   # Target variable

X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

model = RandomForestRegressor()
model.fit(X_train, y_train)

os.makedirs("saved_model", exist_ok=True)
joblib.dump(model, "saved_model/aqi_model.pkl")

print("✅ Model saved to 'saved_model/aqi_model.pkl'")


# import pandas as pd
# from sklearn.impute import SimpleImputer
# from sklearn.ensemble import RandomForestClassifier
# from sklearn.preprocessing import LabelEncoder
# import joblib

# # Load and preprocess the data
# df = pd.read_csv("city_day.csv")

# # Select relevant columns
# features = ["PM2.5", "PM10", "NO", "NO2", "NH3", "CO", "SO2", "O3"]
# target = "AQI_Bucket"
# df = df[features + ["AQI", target]]

# # Drop rows with missing AQI_Bucket
# df = df.dropna(subset=[target])

# # Separate features and target
# X = df[features]
# y = df[target]

# # Impute missing values
# imputer = SimpleImputer(strategy="mean")
# X_imputed = imputer.fit_transform(X)

# # Encode target
# label_encoder = LabelEncoder()
# y_encoded = label_encoder.fit_transform(y)

# # Train a simple model
# model = RandomForestClassifier(n_estimators=100, random_state=42)
# model.fit(X_imputed, y_encoded)

# # Save model and transformers
# joblib.dump(model, "saved_models/aqi_model.pkl")
# joblib.dump(imputer, "saved_models/imputer.pkl")
# joblib.dump(label_encoder, "saved_models/bucket_encoder.pkl")

# print("✅ Model and encoders saved successfully.")

# # model_train.py
# import pandas as pd
# import joblib
# from sklearn.ensemble import RandomForestClassifier
# from sklearn.model_selection import train_test_split
# from sklearn.preprocessing import LabelEncoder
# from sklearn.impute import SimpleImputer

# # Load dataset
# df = pd.read_csv("city_day.csv")

# # Drop column with too many missing values
# df.drop(columns=["Xylene"], inplace=True)

# # Drop rows where target is missing
# df.dropna(subset=["AQI_Bucket"], inplace=True)

# # Fill missing values with median
# features = df.drop(columns=["Date", "AQI", "AQI_Bucket"])
# imputer = SimpleImputer(strategy="median")
# X_imputed = imputer.fit_transform(features.select_dtypes(include=["float64", "int"]))

# # Encode categorical variables
# le_city = LabelEncoder()
# city_encoded = le_city.fit_transform(features["City"])
# X = pd.DataFrame(X_imputed, columns=features.select_dtypes(include=["float64", "int"]).columns)
# X["City"] = city_encoded

# # Encode target
# le_bucket = LabelEncoder()
# y = le_bucket.fit_transform(df["AQI_Bucket"])

# # Train-test split
# X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

# # Train model
# model = RandomForestClassifier(n_estimators=100, random_state=42)
# model.fit(X_train, y_train)

# # Save model and encoders
# joblib.dump(model, "aqi_model.pkl")
# joblib.dump(le_city, "city_encoder.pkl")
# joblib.dump(le_bucket, "bucket_encoder.pkl")
# joblib.dump(imputer, "imputer.pkl")

# print("✅ Model and encoders saved successfully.")
