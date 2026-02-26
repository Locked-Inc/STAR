//! Multi-language documentation source support.
//!
//! Each language module handles parsing its specific documentation format
//! and converting it into a common representation for Typst emission.

pub mod c;
pub mod go;
pub mod protobuf;
pub mod typescript;
