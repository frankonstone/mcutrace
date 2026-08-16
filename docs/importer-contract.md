# Importer contract

mcutrace keeps producer parsing outside the core traceability model. An importer consumes one `ArtifactInput` and returns an `ImportFragment` through `std::expected`.

## Boundary

An importer may be implemented without linking to any producer library. It needs only the mcutrace public model/importer headers and may parse its producer format however the producer chooses. The importer must not run the producer tool.

`ArtifactInput` provides:

- `path`: the artifact identity used for provenance and diagnostics
- `base_directory`: the deterministic base for relative paths
- `content`: the complete artifact payload

An importer returns:

- `InputFormat`: producer/schema/version identification
- typed `Node` values
- typed or custom `Edge` values
- preserved `GenericArtifact` values when lossless retention is useful
- recoverable per-entry `Diagnostic` values

Fatal unreadable/unsupported artifacts return `std::unexpected(ImportError)`.

## Identity and paths

Importer-generated node IDs must be deterministic for identical input. Use `normalize_artifact_path()` before a path participates in node identity. Do not require source files to exist on the machine running mcutrace.

Edges may reference nodes not contained in the same fragment. mcutrace intentionally preserves those dangling references; graph validation decides whether they are acceptable.

## Relationship provenance

Every imported edge should set `Provenance::importer` and `Provenance::artifact`. If the producer exposes a precise source location for a relationship, populate `Provenance::source` and/or `Edge::source`.

Known relationship kinds are `satisfies`, `verifies`, `implements`, `covers`, `reports`, and `relates`. Unknown producer relationship names should use `RelationshipType::custom()` rather than being discarded.

## Versions

If a producer exposes a schema/format version, identify it before semantic import and reject unsupported versions with `ImportErrorCode::unsupported_version`. A producer should not silently reinterpret an unknown version.

## Requirement links

Producers may attach a `requirements` array to evidence entries when their schema supports that extension. The built-in trace-aware importer maps those references to typed edges. For producers that cannot or should not change their output, a separate `mcutrace-links` v1 artifact can connect arbitrary node IDs.

Example sidecar:

```json
{
  "format": "mcutrace-links",
  "version": 1,
  "links": [
    {
      "source": "test:vendor:boot_test",
      "target": "REQ-0042",
      "type": "verifies"
    }
  ]
}
```

## Diagnostic codes

Diagnostic `code` values are machine-facing stable identifiers. Once published for a meaning, keep that code for compatible refinements and introduce a new code for incompatible semantic changes. Human-readable messages may evolve independently.

## Extension fixture

`tests/fixtures/third-party-results.json` and `tests/test_extension_contract.cpp` demonstrate a producer-neutral importer that maps a hypothetical third-party JSON format into the same `ImportFragment` model without changing `Graph`, `Node`, or `Edge`.

## Portability

Importer code should avoid platform-specific filesystem or process assumptions. P10 keeps CI macOS-only to conserve runner time; Linux and Windows CI can be enabled later without changing this contract.
