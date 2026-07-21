import { defineConfig } from 'vite';
import { viteSingleFile } from 'vite-plugin-singlefile';

export default defineConfig({
  plugins: [viteSingleFile()],
  build: {
    // Ensure that assets are inlined as much as possible for file:/// protocol
    cssCodeSplit: false,
    assetsInlineLimit: 100000000,
  }
});
