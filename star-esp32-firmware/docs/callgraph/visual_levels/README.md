# Multi-Level Call Graph Analysis

This directory contains call graphs at different abstraction levels:

## Application Layer View
**File:** `callgraph_application.dot`

Shows only your application code and STAR framework calls

## Framework Layer View
**File:** `callgraph_framework.dot`

Shows application + ESP-IDF framework interactions

## System Layer View
**File:** `callgraph_system.dot`

Shows system calls without low-level macros and libc

## Complete Call Graph
**File:** `callgraph_complete.dot`

Shows every function call including system internals

## Usage

- Start with `application` level for high-level architecture overview
- Use `framework` level to understand ESP-IDF integration
- Examine `system` level for performance analysis
- Use `complete` level for detailed debugging

