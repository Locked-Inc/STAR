# Product Guidelines

## Documentation Tone & Messaging
- **Professional & Academic:** All documentation, reports, and code comments must maintain a clear, technical, and formal tone suitable for a senior engineering capstone project.
- **Clarity over Complexity:** While technical, explanations should be accessible to other engineers. Avoid jargon where a simpler technical term suffices. When using acronyms, spell them out on first use (e.g., "SPI (Serial Peripheral Interface)").

## Design Principles
- **Modularity & Abstraction:** Follow SOLID principles, with particular emphasis on the Dependency Inversion Principle (DIP). Hardware-dependent code must be abstracted behind clean interfaces to ensure the system is modular and testable. All five SOLID principles apply: Single Responsibility, Open/Closed, Liskov Substitution, Interface Segregation, and Dependency Inversion.
- **Safety & Reliability:** The system must prioritize deterministic timing and robust error handling. Failsafes (like watchdog timers and emergency stops) must be implemented and documented. Document interrupt priorities and NMI (Non-Maskable Interrupt) handlers, including the rationale for priority assignments.
- **Simplicity:** Favor straightforward, maintainable implementations. Avoid "clever" code that is difficult for other team members or reviewers to audit.

## Development & Validation
- **Rigorous V-Model:** Follow a structured engineering approach where every requirement is mapped to a specific implementation and a corresponding validation test.
- **Traceability:** Maintain clear links between project requirements, design specifications, and test results.

## Terminology & Naming
- **Inclusive Hardware Standards:** Strictly adhere to OSHWA inclusive terminology. 
    - Use **Controller/Peripheral** instead of Master/Slave.
    - Use **COPI/CIPO** (Controller Out, Peripheral In / Controller In, Peripheral Out) for SPI data lines.
- **Functional Naming:** Name modules and variables based on their specific role within the STAR ecosystem (e.g., `MotorGateway`, `DriveController`) to ensure the intent is immediately obvious.

## Visual & Technical Standards
- **Engineering Excellence:** Schematics must follow professional EDA standards (clear labeling, logical flow, power/ground conventions). LaTeX documentation must adhere to formal academic reporting styles.
- **Visual Integration:** Complement technical text with high-quality block diagrams, flowcharts, and 3D renders to facilitate rapid understanding of complex system interactions.
