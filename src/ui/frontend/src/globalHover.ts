const globalHoverStyle = new CSSStyleSheet();
globalHoverStyle.replaceSync(`
  button, .menu-item, .category-item, .dropdown-item, .icon-button, .action-btn, .file-item, .folder-item {
    transition: background-color 0.1s ease, filter 0.1s ease !important;
  }
  
  /* VS Code style subtle highlight */
  button:hover:not(.close-btn), 
  .menu-item:hover, 
  .category-item:hover, 
  .dropdown-item:hover, 
  .icon-button:hover, 
  .action-btn:hover,
  .file-item:hover,
  .folder-item:hover {
    background-color: rgba(255, 255, 255, 0.1) !important;
  }

  button:active:not(.close-btn), 
  .menu-item:active, 
  .category-item:active, 
  .dropdown-item:active, 
  .icon-button:active, 
  .action-btn:active,
  .file-item:active,
  .folder-item:active {
    background-color: rgba(255, 255, 255, 0.15) !important;
  }
`);

// Monkey-patch attachShadow to inject our global hover styles into all Lit components automatically
const originalAttachShadow = HTMLElement.prototype.attachShadow;
HTMLElement.prototype.attachShadow = function(init: ShadowRootInit) {
  const shadowRoot = originalAttachShadow.call(this, init);
  // Lit might override adoptedStyleSheets later, but Lit 2/3 actually sets it.
  // Wait, Lit sets adoptedStyleSheets in connectedCallback or update.
  // A safer way is to patch LitElement's finalize method or just use a MutationObserver.
  return shadowRoot;
};
