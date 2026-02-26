//! Typst source file emitter modules.
//!
//! Transforms parsed Doxygen data structures into Typst markup for
//! compilation into a unified PDF document.

pub mod code;
pub mod compound;
pub mod crossref;
pub mod document;
pub mod file;
pub mod function;
pub mod graph;
pub mod index_page;
pub mod template;

pub use document::DocumentEmitter;
