import { mount } from 'svelte'
import './app.css'
import App from './App.svelte'
import FloatingPanel from './FloatingPanel.svelte'

// Check if this is a floating panel window
const isFloatingPanel = window.location.hash.startsWith('#/floating/');

// Mount the appropriate component
const app = mount(isFloatingPanel ? FloatingPanel : App, {
  target: document.getElementById('app'),
})

export default app
