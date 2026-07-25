import { css } from 'lit';

export const globalHoverStyle = css`
  button, .menu-item, .category-item, .dropdown-item, .icon-button, .action-btn, .file-item, .folder-item {
    transition: background-color 0.1s ease, filter 0.1s ease;
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
    background-color: rgba(255, 255, 255, 0.1);
  }

  button:active:not(.close-btn), 
  .menu-item:active, 
  .category-item:active, 
  .dropdown-item:active, 
  .icon-button:active, 
  .action-btn:active,
  .file-item:active,
  .folder-item:active {
    background-color: rgba(255, 255, 255, 0.15);
  }
`;
