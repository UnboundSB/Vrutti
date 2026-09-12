// vite.config.ts
import { defineConfig } from "file:///D:/vrutti/vrutti_ide/src/ui/frontend/node_modules/vite/dist/node/index.js";
import { viteSingleFile } from "file:///D:/vrutti/vrutti_ide/src/ui/frontend/node_modules/vite-plugin-singlefile/dist/esm/index.js";
import { viteStaticCopy } from "file:///D:/vrutti/vrutti_ide/src/ui/frontend/node_modules/vite-plugin-static-copy/dist/index.js";
var vite_config_default = defineConfig({
  plugins: [
    viteSingleFile(),
    viteStaticCopy({
      targets: [
        {
          src: "node_modules/material-icon-theme/icons/*.svg",
          dest: "icons"
        }
      ]
    })
  ],
  build: {
    // Ensure that assets are inlined as much as possible for file:/// protocol
    cssCodeSplit: false,
    assetsInlineLimit: 1e8
  }
});
export {
  vite_config_default as default
};
//# sourceMappingURL=data:application/json;base64,ewogICJ2ZXJzaW9uIjogMywKICAic291cmNlcyI6IFsidml0ZS5jb25maWcudHMiXSwKICAic291cmNlc0NvbnRlbnQiOiBbImNvbnN0IF9fdml0ZV9pbmplY3RlZF9vcmlnaW5hbF9kaXJuYW1lID0gXCJEOlxcXFx2cnV0dGlcXFxcdnJ1dHRpX2lkZVxcXFxzcmNcXFxcdWlcXFxcZnJvbnRlbmRcIjtjb25zdCBfX3ZpdGVfaW5qZWN0ZWRfb3JpZ2luYWxfZmlsZW5hbWUgPSBcIkQ6XFxcXHZydXR0aVxcXFx2cnV0dGlfaWRlXFxcXHNyY1xcXFx1aVxcXFxmcm9udGVuZFxcXFx2aXRlLmNvbmZpZy50c1wiO2NvbnN0IF9fdml0ZV9pbmplY3RlZF9vcmlnaW5hbF9pbXBvcnRfbWV0YV91cmwgPSBcImZpbGU6Ly8vRDovdnJ1dHRpL3ZydXR0aV9pZGUvc3JjL3VpL2Zyb250ZW5kL3ZpdGUuY29uZmlnLnRzXCI7aW1wb3J0IHsgZGVmaW5lQ29uZmlnIH0gZnJvbSAndml0ZSc7XHJcbmltcG9ydCB7IHZpdGVTaW5nbGVGaWxlIH0gZnJvbSAndml0ZS1wbHVnaW4tc2luZ2xlZmlsZSc7XHJcbmltcG9ydCB7IHZpdGVTdGF0aWNDb3B5IH0gZnJvbSAndml0ZS1wbHVnaW4tc3RhdGljLWNvcHknO1xyXG5cclxuZXhwb3J0IGRlZmF1bHQgZGVmaW5lQ29uZmlnKHtcclxuICBwbHVnaW5zOiBbXHJcbiAgICB2aXRlU2luZ2xlRmlsZSgpLFxyXG4gICAgdml0ZVN0YXRpY0NvcHkoe1xyXG4gICAgICB0YXJnZXRzOiBbXHJcbiAgICAgICAge1xyXG4gICAgICAgICAgc3JjOiAnbm9kZV9tb2R1bGVzL21hdGVyaWFsLWljb24tdGhlbWUvaWNvbnMvKi5zdmcnLFxyXG4gICAgICAgICAgZGVzdDogJ2ljb25zJ1xyXG4gICAgICAgIH1cclxuICAgICAgXVxyXG4gICAgfSlcclxuICBdLFxyXG4gIGJ1aWxkOiB7XHJcbiAgICAvLyBFbnN1cmUgdGhhdCBhc3NldHMgYXJlIGlubGluZWQgYXMgbXVjaCBhcyBwb3NzaWJsZSBmb3IgZmlsZTovLy8gcHJvdG9jb2xcclxuICAgIGNzc0NvZGVTcGxpdDogZmFsc2UsXHJcbiAgICBhc3NldHNJbmxpbmVMaW1pdDogMTAwMDAwMDAwLFxyXG4gIH1cclxufSk7XHJcbiJdLAogICJtYXBwaW5ncyI6ICI7QUFBMFMsU0FBUyxvQkFBb0I7QUFDdlUsU0FBUyxzQkFBc0I7QUFDL0IsU0FBUyxzQkFBc0I7QUFFL0IsSUFBTyxzQkFBUSxhQUFhO0FBQUEsRUFDMUIsU0FBUztBQUFBLElBQ1AsZUFBZTtBQUFBLElBQ2YsZUFBZTtBQUFBLE1BQ2IsU0FBUztBQUFBLFFBQ1A7QUFBQSxVQUNFLEtBQUs7QUFBQSxVQUNMLE1BQU07QUFBQSxRQUNSO0FBQUEsTUFDRjtBQUFBLElBQ0YsQ0FBQztBQUFBLEVBQ0g7QUFBQSxFQUNBLE9BQU87QUFBQTtBQUFBLElBRUwsY0FBYztBQUFBLElBQ2QsbUJBQW1CO0FBQUEsRUFDckI7QUFDRixDQUFDOyIsCiAgIm5hbWVzIjogW10KfQo=
