<script>
  import { onMount, onDestroy } from 'svelte';
  import { invoke } from '@tauri-apps/api/core';

  let consoleOutput = $state([]);
  let consoleInput = $state('');
  let commandHistory = $state([]);
  let historyIndex = $state(-1);
  let consoleScrollContainer;
  let autoScroll = $state(true);

  onMount(() => {
    addConsoleOutput('system', 'Terminal ready. Type commands or view system output.');
    addConsoleOutput('info', 'Available commands: help, clear, status');
  });

  function addConsoleOutput(type, text) {
    consoleOutput.push({
      timestamp: Date.now(),
      type,
      text
    });
    consoleOutput = consoleOutput;

    if (autoScroll && consoleScrollContainer) {
      setTimeout(() => {
        consoleScrollContainer.scrollTop = consoleScrollContainer.scrollHeight;
      }, 0);
    }
  }

  async function handleConsoleCommand(event) {
    if (event.key === 'Enter' && consoleInput.trim()) {
      const command = consoleInput.trim();
      addConsoleOutput('command', `> ${command}`);

      commandHistory.push(command);
      historyIndex = commandHistory.length;

      // Handle commands
      if (command === 'clear') {
        consoleOutput = [];
      } else if (command === 'help') {
        addConsoleOutput('info', 'Available commands:');
        addConsoleOutput('info', '  help - Show this help message');
        addConsoleOutput('info', '  clear - Clear console output');
        addConsoleOutput('info', '  status - Show device status');
      } else if (command === 'status') {
        addConsoleOutput('info', 'Device status: Connected');
      } else {
        addConsoleOutput('error', `Unknown command: ${command}`);
      }

      consoleInput = '';
    } else if (event.key === 'ArrowUp') {
      event.preventDefault();
      if (historyIndex > 0) {
        historyIndex--;
        consoleInput = commandHistory[historyIndex];
      }
    } else if (event.key === 'ArrowDown') {
      event.preventDefault();
      if (historyIndex < commandHistory.length - 1) {
        historyIndex++;
        consoleInput = commandHistory[historyIndex];
      } else {
        historyIndex = commandHistory.length;
        consoleInput = '';
      }
    }
  }

  function handleClear() {
    consoleOutput = [];
    addConsoleOutput('system', 'Console cleared');
  }

  function toggleAutoScroll() {
    autoScroll = !autoScroll;
  }

  function exportConsole() {
    const text = consoleOutput.map(line => {
      const date = new Date(line.timestamp);
      const time = date.toTimeString().split(' ')[0];
      return `[${time}] [${line.type}] ${line.text}`;
    }).join('\n');

    const blob = new Blob([text], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `terminal-${Date.now()}.txt`;
    a.click();
    URL.revokeObjectURL(url);
    addConsoleOutput('success', 'Terminal output exported');
  }
</script>

<div class="terminal-panel">
  <div class="terminal-header">
    <div class="terminal-title">Terminal</div>
    <div class="terminal-controls">
      <button class="control-btn" onclick={handleClear} title="Clear terminal">
        Clear
      </button>
      <button
        class="control-btn"
        class:active={autoScroll}
        onclick={toggleAutoScroll}
        title="Toggle auto-scroll"
      >
        Auto-scroll
      </button>
      <button class="control-btn" onclick={exportConsole} title="Export to file">
        Export
      </button>
    </div>
  </div>

  <div class="terminal-content" bind:this={consoleScrollContainer}>
    {#if consoleOutput.length === 0}
      <div class="terminal-empty">
        <p>No output yet. Type a command below.</p>
      </div>
    {:else}
      {#each consoleOutput as line}
        <div class="console-line console-{line.type}">
          <span class="console-timestamp">{new Date(line.timestamp).toLocaleTimeString()}</span>
          <span class="console-text">{line.text}</span>
        </div>
      {/each}
    {/if}
  </div>

  <div class="terminal-input">
    <span class="terminal-prompt">$</span>
    <input
      type="text"
      bind:value={consoleInput}
      onkeydown={handleConsoleCommand}
      placeholder="Type command..."
      autocomplete="off"
    />
  </div>
</div>

<style>
  .terminal-panel {
    display: flex;
    flex-direction: column;
    height: 100%;
    width: 100%;
    background: var(--bg-primary, #FFFFFF);
    font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
  }

  .terminal-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 8px 16px;
    background: var(--bg-secondary, #F8F9FA);
    border-bottom: 1px solid var(--border-light, #E0E0E0);
  }

  .terminal-title {
    font-weight: 600;
    font-size: 13px;
    color: var(--text-primary, #1F2937);
  }

  .terminal-controls {
    display: flex;
    gap: 8px;
  }

  .control-btn {
    padding: 4px 12px;
    font-size: 12px;
    border: 1px solid var(--border-light, #E0E0E0);
    background: var(--bg-primary, #FFFFFF);
    border-radius: 4px;
    cursor: pointer;
    transition: all 0.15s ease;
  }

  .control-btn:hover {
    background: var(--bg-secondary, #F8F9FA);
    border-color: var(--color-accent-500, #2563EB);
  }

  .control-btn.active {
    background: var(--color-accent-500, #2563EB);
    color: white;
    border-color: var(--color-accent-500, #2563EB);
  }

  .terminal-content {
    flex: 1;
    overflow-y: auto;
    padding: 12px;
    font-size: 12px;
    line-height: 1.6;
    background: var(--bg-primary, #FFFFFF);
    color: var(--text-primary, #1F2937);
  }

  .terminal-empty {
    display: flex;
    align-items: center;
    justify-content: center;
    height: 100%;
    color: var(--text-secondary, #6B7280);
    font-style: italic;
  }

  .console-line {
    display: flex;
    gap: 12px;
    padding: 2px 0;
  }

  .console-timestamp {
    color: var(--text-secondary, #6B7280);
    font-size: 10px;
    min-width: 80px;
    flex-shrink: 0;
  }

  .console-text {
    flex: 1;
    white-space: pre-wrap;
    word-break: break-word;
  }

  .console-line.command .console-text {
    color: var(--color-accent-600, #2563EB);
    font-weight: 600;
  }

  .console-line.error .console-text {
    color: var(--color-danger-500, #EF4444);
  }

  .console-line.success .console-text {
    color: var(--color-success-500, #22C55E);
  }

  .console-line.info .console-text {
    color: var(--color-accent-500, #3B82F6);
  }

  .console-line.system .console-text {
    color: var(--color-warning-600, #D97706);
  }

  .terminal-input {
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 8px 12px;
    border-top: 1px solid var(--border-light, #E0E0E0);
    background: var(--bg-secondary, #F8F9FA);
  }

  .terminal-prompt {
    color: var(--color-accent-600, #2563EB);
    font-weight: bold;
    font-size: 14px;
  }

  .terminal-input input {
    flex: 1;
    background: transparent;
    border: none;
    color: var(--text-primary, #1F2937);
    font-family: inherit;
    font-size: 12px;
    outline: none;
  }

  .terminal-input input::placeholder {
    color: var(--text-secondary, #6B7280);
  }
</style>
