//! C/C++ documentation handling via Doxygen XML.
//!
//! This is the default path: Doxygen generates XML, the `doxygen` module
//! parses it, and the `typst_emitter` module emits Typst.  This module
//! provides any C-specific processing or filtering.

use crate::doxygen::compound::CompoundDef;

/// Filter out internal/private compounds from the documentation.
pub fn filter_public_compounds(compounds: Vec<CompoundDef>) -> Vec<CompoundDef> {
    compounds
        .into_iter()
        .filter(|c| {
            // Skip directory compounds
            c.kind != crate::doxygen::CompoundKind::Dir
        })
        .collect()
}

/// Sort compounds for documentation output: headers first, then source files.
pub fn sort_compounds(compounds: &mut [CompoundDef]) {
    compounds.sort_by(|a, b| {
        let a_is_header = a.name.ends_with(".h") || a.name.ends_with(".hpp");
        let b_is_header = b.name.ends_with(".h") || b.name.ends_with(".hpp");
        match (a_is_header, b_is_header) {
            (true, false) => std::cmp::Ordering::Less,
            (false, true) => std::cmp::Ordering::Greater,
            _ => a.name.cmp(&b.name),
        }
    });
}
