import { defineConfig } from 'vite'
import preact from '@preact/preset-vite'
import compression from 'vite-plugin-compression'

export default defineConfig({
  plugins: [
    preact(),
    compression({ algorithm: 'gzip' })
  ],
  server: {
    // Forward API calls to tools/test_server (or a real device) during `npm run dev`
    proxy: {
      '/api': {
        target: 'http://127.0.0.1:8080',
        changeOrigin: true,
      }
    },
  },
  build: {
    outDir: 'dist',
    emptyOutDir: true,
    minify: 'esbuild',
    rollupOptions: {
      output: {
        manualChunks: undefined,
        inlineDynamicImports: true,
        entryFileNames: 'assets/app.js',
        chunkFileNames: 'assets/[name].js',
        assetFileNames: 'assets/[name].[ext]'
      }
    }
  }
})
