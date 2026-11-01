/** @type {import('tailwindcss').Config} */
module.exports = {
  content: ["./index.html", "./src/renderer/src/**/*.{ts,tsx}"],
  theme: {
    extend: {
      colors: {
        abyss: "#07080c",
        siren: "#ff1e27",
        police: "#0066ff",
      },
      fontFamily: {
        display: ["Oswald", "Arial Narrow", "sans-serif"],
        heading: ["Oswald", "Arial Narrow", "sans-serif"],
        body: ["Inter", "Arial", "sans-serif"],
      },
      animation: {
        "siren-breathe": "siren-breathe 9s ease-in-out infinite alternate",
        "wanted-flash": "wanted-flash 1.8s steps(2, end) infinite",
      },
      keyframes: {
        "siren-breathe": {
          "0%, 100%": { opacity: "0.5", transform: "scale(1)" },
          "50%": { opacity: "0.85", transform: "scale(1.12)" },
        },
        "wanted-flash": {
          "0%, 100%": { color: "#ff1e27", textShadow: "0 0 12px #ff1e27" },
          "50%": { color: "#00a3ff", textShadow: "0 0 12px #00a3ff" },
        },
      },
    },
  },
  plugins: [],
};
