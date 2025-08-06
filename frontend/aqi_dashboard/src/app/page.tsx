"use client";

import { useEffect, useState } from "react";
import AQIBar from "@/components/AQIBar";
import axios from "axios";

type AQIResponse = {
  aqi: number;
  status: string;
  sensor_data: {
    "PM2.5": number;
    "PM10": number;
    NH3: number;
    CO: number;
    temp?: number; // Optional
    hum?: number; // Optional
  };
};

export default function Dashboard() {
  const [data, setData] = useState<AQIResponse | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [hasMounted, setHasMounted] = useState(false);
  const [healthStatus, setHealthStatus] = useState<any>(null);

  useEffect(() => {
    setHasMounted(true);
  }, []);

  useEffect(() => {
    const checkHealth = async () => {
      try {
        const healthResponse = await axios.get("http://localhost:8000/api/health");
        setHealthStatus(healthResponse.data);
      } catch (err) {
        setHealthStatus(null);
      }
    };

    const fetchData = async () => {
      try {
        const response = await axios.get("http://localhost:8000/api/aqi");
        setData(response.data);
        setError(null);
      } catch (err: any) {
        const errorMessage = err.response?.data?.detail || err.message || "Error fetching data from Arduino";
        setError(`Error: ${errorMessage}`);
        console.error("Error:", err);
      }
    };

    checkHealth();
    fetchData();
    const interval = setInterval(fetchData, 5000);
    return () => clearInterval(interval);
  }, []);

  if (!hasMounted) return null; // Prevent hydration mismatch

  return (
    <main className="min-h-screen bg-gradient-to-br from-white to-gray-300 p-6">
      <h1 className="text-3xl font-bold mb-6">
        🌍 Air Quality Index Monitoring System
      </h1>

      {healthStatus && (
        <div className="bg-blue-100 border border-blue-400 text-blue-700 px-4 py-3 rounded mb-4">
          <strong>Backend Status:</strong> 
          Serial: {healthStatus.serial_connected ? "✅ Connected" : "❌ Disconnected"} | 
          Model: {healthStatus.model_loaded ? "✅ Loaded" : "❌ Not Loaded"}
        </div>
      )}

      {error && (
        <div className="bg-red-100 border border-red-400 text-red-700 px-4 py-3 rounded mb-4">
          {error}
        </div>
      )}

      {data ? (
        <>
          <div className="mb-6">
            <AQIBar value={data.aqi} />
          </div>
          <div className="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 gap-4 text-sm">
            <Card label="AQI" value={data.aqi} />
            <Card label="Status" value={data.status} />
            <Card label="PM2.5" value={`${data.sensor_data["PM2.5"]} µg/m³`} />
            <Card label="PM10" value={`${data.sensor_data["PM10"]} µg/m³`} />
            <Card label="NH3" value={`${data.sensor_data.NH3} µg/m³`} />
            <Card label="CO" value={`${data.sensor_data.CO} mg/m³`} />
            {data.sensor_data.temp !== undefined && data.sensor_data.temp !== null && (
              <Card label="Temperature" value={`${data.sensor_data.temp} °C`} />
            )}
            {data.sensor_data.hum !== undefined && data.sensor_data.hum !== null && (
              <Card label="Humidity" value={`${data.sensor_data.hum} %`} />
            )}
          </div>
        </>
      ) : (
        <div className="text-center py-8">
          <p className="text-gray-600 mb-4">Loading data...</p>
          {!healthStatus && (
            <p className="text-red-600">⚠️ Backend server not running. Please start the backend server.</p>
          )}
        </div>
      )}
    </main>
  );
}

const getAQIColor = (label: string, value: string | number) => {
  if (label === "AQI") {
    const aqi = Number(value);
    if (aqi <= 50) return "bg-green-300";
    if (aqi <= 100) return "bg-yellow-300";
    if (aqi <= 200) return "bg-orange-300";
    if (aqi <= 300) return "bg-red-300";
    if (aqi <= 400) return "bg-purple-300";
    return "bg-gray-100";
  }
  if (label === "Status") {
    const status = String(value).toLowerCase();
    if (status.includes("good")) return "bg-green-300";
    if (status.includes("satisfactory")) return "bg-yellow-300";
    if (status.includes("moderate")) return "bg-orange-300";
    if (status.includes("poor")) return "bg-red-300";
    if (status.includes("very poor")) return "bg-purple-300";
    if (status.includes("severe")) return "bg-gray-100";
  }
  return "bg-white";
};

const Card = ({ label, value }: { label: string; value: string | number }) => {
  const bgColor = getAQIColor(label, value);
  
  return (
    <div className={`${bgColor} shadow-md rounded-lg p-4 border border-gray-100 hover:border-green-300 transition-all duration-300`}>
      <p className="text-gray-700 font-medium">{label}</p>
      <p className="font-semibold text-lg text-gray-900">{value}</p>
    </div>
  );
};



// "use client";

// import { useEffect, useState } from "react";
// import AQIBar from "@/components/AQIBar";
// import axios from "axios";

// type AQIResponse = {
//   aqi: number;
//   status: string;
//   sensor_data: {
//     "PM2.5": number;
//     "PM10": number;
//     NH3: number;
//     CO: number;
//     temp?: number;
//     hum?: number;
//   };
// };
// export default function Dashboard() {
//   const [data, setData] = useState<AQIResponse | null>(null);
//   const [error, setError] = useState<string | null>(null);
//   const [hasMounted, setHasMounted] = useState(false);

//   useEffect(() => {
//     setHasMounted(true);
//   }, []);

//   useEffect(() => {
//     const fetchData = async () => {
//       try {
//         const response = await axios.get("http://localhost:8000/api/aqi");
//         setData(response.data);
//         setError(null);
//       } catch (err) {
//         setError("Error fetching data from Arduino");
//         console.error("Error:", err);
//       }
//     };

//     fetchData();
//     const interval = setInterval(fetchData, 5000);
//     return () => clearInterval(interval);
//   }, []);

//   if (!hasMounted) return null; // 🔥 Prevent hydration mismatch

//   return (
//     <main className="min-h-screen bg-gradient-to-br from-white to-gray-300 p-6">
//       <h1 className="text-3xl font-bold mb-6">
//         🌍 Air Quality Index Monitoring System
//       </h1>

//       {error && (
//         <div className="bg-red-100 border border-red-400 text-red-700 px-4 py-3 rounded mb-4">
//           {error}
//         </div>
//       )}

//       {data ? (
//         <>
//           <div className="mb-6">
//             <AQIBar value={data.aqi} />
//           </div>
//           <div className="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 gap-4 text-sm">
//             <Card label="AQI" value={data.aqi} />
//             <Card label="Status" value={data.status} />
//             <Card label="PM2.5" value={`${data.sensor_data["PM2.5"]} µg/m³`} />
//             <Card label="PM10" value={`${data.sensor_data["PM10"]} µg/m³`} />
//             <Card label="NH3" value={`${data.sensor_data.NH3} µg/m³`} />
//             <Card label="CO" value={`${data.sensor_data.CO} mg/m³`} />
//             {data.sensor_data.temp !== undefined && data.sensor_data.temp !== null && (
//         <Card label="Temperature" value={`${data.sensor_data.temp} °C`} />
//       )}
//       {data.sensor_data.hum !== undefined && data.sensor_data.hum !== null && (
//         <Card label="Humidity" value={`${data.sensor_data.hum} %`} />
//       )}
//           </div>
//         </>
//       ) : (
//         <p>Loading data...</p>
//       )}
//     </main>
//   );
// }


// // export default function Dashboard() {
// //   const [data, setData] = useState<AQIResponse | null>(null);
// //   const [error, setError] = useState<string | null>(null);

// //   useEffect(() => {
// //     const fetchData = async () => {
// //       try {
// //         const response = await axios.get("http://localhost:8000/api/aqi");
// //         setData(response.data);
// //         setError(null);
// //       } catch (err) {
// //         setError("Error fetching data from Arduino");
// //         console.error("Error:", err);
// //       }
// //     };

// //     // Initial fetch
// //     fetchData();

// //     // Set up polling every 5 seconds
// //     const interval = setInterval(fetchData, 5000);

// //     // Cleanup interval on component unmount
// //     return () => clearInterval(interval);
// //   }, []);

// //   return (
// //     <main className="min-h-screen bg-gradient-to-br from-white to-gray-300 p-6">
// //       <h1 className="text-3xl font-bold mb-6">
// //         🌍 Air Quality Index Monitoring System
// //       </h1>

// //       {error && (
// //         <div className="bg-red-100 border border-red-400 text-red-700 px-4 py-3 rounded mb-4">
// //           {error}
// //         </div>
// //       )}

// //       {data ? (
// //         <>
// //           <div className="mb-6">
// //             <AQIBar value={data.aqi} />
// //           </div>
// //           <div className="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 gap-4 text-sm">
// //           <Card label="PM2.5" value={`${data.sensor_data["PM2.5"]} µg/m³`} />
// // <Card label="PM10" value={`${data.sensor_data["PM10"]} µg/m³`} />
// // <Card label="NH3" value={`${data.sensor_data.NH3} µg/m³`} />
// // <Card label="CO" value={`${data.sensor_data.CO} mg/m³`} />
// // <Card label="Temperature" value={`${data.sensor_data.temp} °C`} />
// // <Card label="Humidity" value={`${data.sensor_data.hum} %`} />
// //           </div>
// //         </>
// //       ) : (
// //         <p>Loading data...</p>
// //       )}
// //     </main>
// //   );
// // }

// const getAQIColor = (label: string, value: string | number) => {
//   if (label === "AQI") {
//     const aqi = Number(value);
//     if (aqi <= 50) return "bg-green-300";
//     if (aqi <= 100) return "bg-yellow-300";
//     if (aqi <= 200) return "bg-orange-300";
//     if (aqi <= 300) return "bg-red-300";
//     if (aqi <= 400) return "bg-purple-300";
//     return "bg-gray-100";
//   }
//   if (label === "Status") {
//     const status = String(value).toLowerCase();
//     if (status.includes("good")) return "bg-green-300";
//     if (status.includes("satisfactory")) return "bg-yellow-300";
//     if (status.includes("moderate")) return "bg-orange-300";
//     if (status.includes("poor")) return "bg-red-300";
//     if (status.includes("very poor")) return "bg-purple-300";
//     if (status.includes("severe")) return "bg-gray-100";
//   }
//   return "bg-white";
// };

// const Card = ({ label, value }: { label: string; value: string | number }) => {
//   const bgColor = getAQIColor(label, value);
  
//   return (
//     <div className={`${bgColor} shadow-md rounded-lg p-4 border border-gray-100 hover:border-green-300 transition-all duration-300`}>
//       <p className="text-gray-700 font-medium">{label}</p>
//       <p className="font-semibold text-lg text-gray-900">{value}</p>
//     </div>
//   );
// };

