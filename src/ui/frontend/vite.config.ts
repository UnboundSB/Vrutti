import { defineConfig } from 'vite';
import { viteSingleFile } from 'vite-plugin-singlefile';
import { viteStaticCopy } from 'vite-plugin-static-copy';

export default defineConfig({
  plugins: [
    viteSingleFile(),
    viteStaticCopy({
      targets: [
        {
          src: 'node_modules/material-icon-theme/icons/*.svg',
          dest: 'icons'
        }
      ]
    })
  ],
  build: {
    // Ensure that assets are inlined as much as possible for file:/// protocol
    cssCodeSplit: false,
    assetsInlineLimit: 100000000,
  }
});
