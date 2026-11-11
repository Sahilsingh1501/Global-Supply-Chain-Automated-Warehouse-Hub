import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

export default defineConfig({
  plugins: [react()],
  server: {
    host: "0.0.0.0",
    port: Number(process.env.VITE_PORT || 5173),
    strictPort: true,
    proxy: {
      "/api": "http://127.0.0.1:4000",
    },
  },
  build: {
    rollupOptions: {
      output: {
        manualChunks(id) {
          if (id.includes("node_modules/recharts")) return "charts";
          if (id.includes("node_modules/react")) return "react-vendor";
        },
      },
    },
  },
});
