---
trigger: always_on
---

# Universal Codebase Engineering Rules

These rules apply to every repository, project, service, application, automation, script, backend, frontend, library, and infrastructure component.

---

## 1. Fail Fast and Fail Loud

* Never hide failures.
* Never silently ignore exceptions.
* Never continue execution after critical failures.
* Missing dependencies, configurations, files, environment variables, credentials, or required resources must immediately raise explicit errors.
* Every failure must be visible through logs and terminal output.

---

## 2. Single Source of Truth

* Information must exist in one authoritative location.
* Avoid duplicated configuration, business logic, constants, schemas, and validation rules.
* Shared logic must be centralized and reused.
* Changes should only need to be made once.

---

## 3. No Hardcoded Values

* Configuration belongs in configuration systems.
* Secrets belong in secret management systems.
* Environment-specific values belong in environment configuration.
* Business rules must not be hidden inside source code.

---

## 4. Simplicity Over Cleverness

* Prefer readable code over clever code.
* Prefer maintainable solutions over impressive solutions.
* Every future developer should understand the implementation quickly.
* If a simpler solution exists, use it.

---

## 5. Reuse Before Creating

Before adding:

* New files
* New classes
* New utilities
* New services
* New abstractions

Verify an existing implementation cannot be extended safely.

Avoid creating parallel systems.

---

## 6. Keep Architecture Consistent

* Similar problems should be solved similarly.
* Naming conventions must remain consistent.
* Project structure must remain predictable.
* Patterns should not change unnecessarily between modules.

---

## 7. Production Quality Only

* No temporary fixes.
* No placeholder implementations.
* No unfinished code paths.
* No TODO-driven architecture.
* Every committed change should be deployable.

---

## 8. Eliminate Technical Debt Immediately

Whenever touching code:

* Remove dead code.
* Remove unused imports.
* Remove obsolete comments.
* Remove abandoned logic.
* Remove stale documentation.

Always leave the area cleaner than before.

---

## 9. Strong Contracts Between Components

* Validate inputs.
* Validate outputs.
* Validate schemas.
* Validate interfaces.

No component should assume another component behaved correctly.

Trust through verification.

---

## 10. Security by Default

* Deny by default.
* Validate all external input.
* Sanitize user-controlled data.
* Follow least-privilege principles.
* Protect secrets and credentials.
* Never expose sensitive information through logs or errors.

---

## 11. Observability First

Every important system should provide:

* Meaningful logs
* Health visibility
* Error visibility
* Performance visibility

Problems should be discoverable without guesswork.

---

## 12. Performance Without Premature Optimization

* Measure before optimizing.
* Optimize bottlenecks, not assumptions.
* Keep implementations efficient and maintainable.
* Avoid unnecessary complexity in pursuit of theoretical gains.

---

## 13. Automation Over Manual Work

* Automate repetitive tasks.
* Automate validation.
* Automate testing.
* Automate formatting.
* Automate quality checks.

Humans should focus on decisions, not repetition.

---

## 14. Minimize Dependencies

* Every dependency introduces risk.
* Prefer existing project capabilities before adding packages.
* Add dependencies only when they provide clear long-term value.
* Remove unused dependencies immediately.

---

## 15. Documentation Must Match Reality

* Documentation is part of the product.
* Update documentation when behavior changes.
* Remove outdated information.
* Never allow documentation to drift from implementation.

---

## 16. Backward Compatibility Matters

* Understand downstream impact before changing interfaces.
* Avoid breaking consumers unnecessarily.
* Version intentionally.
* Migrate responsibly.

---

## 17. Test What Matters

* Test critical business logic.
* Test failure paths.
* Test integrations.
* Test edge cases.
* Verify behavior rather than implementation details.

---

## 18. Keep Responses and Outputs Concise

* Communicate clearly.
* Avoid unnecessary verbosity.
* Deliver information efficiently.
* Prefer signal over noise.

---

## 19. Leave the System Better Than You Found It

Every change should improve at least one of:

* Reliability
* Readability
* Security
* Performance
* Maintainability
* Documentation
* Testability

No commit should knowingly degrade the codebase.

---

## Engineering Principles

1. Correctness before speed.
2. Reliability before features.
3. Security before convenience.
4. Simplicity before complexity.
5. Reuse before creation.
6. Automation before manual effort.
7. Maintainability before cleverness.
8. Visibility before assumptions.
9. Quality before quantity.
10. Continuous improvement over temporary fixes.
