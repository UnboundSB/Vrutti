import { css } from 'lit';

export const globalHoverStyle = css`
  /* Custom Theme-Centric Scrollbar */
  ::-webkit-scrollbar {
    width: 12px;
    height: 12px;
  }
  ::-webkit-scrollbar-track {
    background: var(--vrutti-surface, rgba(0, 0, 0, 0.2));
    border-radius: 4px;
  }
  ::-webkit-scrollbar-thumb {
    background: var(--vrutti-surface-border, rgba(255, 255, 255, 0.1));
    border-radius: 4px;
    border: 2px solid var(--vrutti-bg, transparent); /* creates padding effect */
  }
  ::-webkit-scrollbar-thumb:hover {
    background: var(--vrutti-accent, rgba(255, 255, 255, 0.2));
  }
  ::-webkit-scrollbar-corner {
    background: transparent;
  }

  button, .menu-item, .category-item, .dropdown-item, .icon-button, .action-btn, .file-item, .folder-item {
    transition: background-color 0.1s ease, filter 0.1s ease;
  }
  
  /* Code Editor style subtle highlight */
  button:hover:not(.close-btn), 
  .menu-item:hover, 
  .category-item:hover, 
  .dropdown-item:hover, 
  .icon-button:hover, 
  .action-btn:hover,
  .file-item:hover,
  .folder-item:hover,
  .tree-node:hover {
    background-color: rgba(255, 255, 255, 0.1);
  }

  button:active:not(.close-btn), 
  .menu-item:active, 
  .category-item:active, 
  .dropdown-item:active, 
  .icon-button:active, 
  .action-btn:active,
  .file-item:active,
  .folder-item:active,
  .tree-node:active {
    background-color: rgba(255, 255, 255, 0.15);
  }
`;

