import React from "react";

const getAQIStatus = (aqi: number) => {
  if (aqi <= 50) return { label: "✅ Good", color: "bg-green-500" };
  if (aqi <= 100) return { label: "⚠️ Moderate", color: "bg-yellow-400" };
  if (aqi <= 150) return { label: "🟠 Unhealthy (Sensitive)", color: "bg-orange-400" };
  if (aqi <= 200) return { label: "🔴 Unhealthy", color: "bg-red-500" };
  if (aqi <= 300) return { label: "🔴 Very Unhealthy", color: "bg-purple-700" };
  return { label: "☠️ Hazardous", color: "bg-red-900" };
};

export default function AQIBar({ value }: { value: number }) {
  const { label, color } = getAQIStatus(value);

  return (
    <div>
      <div className={`inline-block px-4 py-2 rounded-full text-white ${color}`}>
        AQI: {value} - {label}
      </div>

      <div className="mt-3 h-4 w-full bg-gray-200 rounded-full overflow-hidden">
        <div
          className={`${color} h-4 transition-all duration-500`}
          style={{ width: `${Math.min(value / 5, 100)}%` }}
        ></div>
      </div>
    </div>
  );
}
