# Changelog

## Unreleased

- Move into silk-lang/nonstd as `native/gpu/metal` (package `gpu-metal`; examples `gpu-metal-examples-compute`, `gpu-metal-examples-headless-render`).
- Pin to Silk `0.1.0-dev.29+94cf9559bf47` and follow the dev.27 stdlib moves (`silk.mem.pointer`, `silk.core.ownership`).
- Pin the library and examples to Silk `0.1.0-dev.17+1120049495d3`.
- Migrate manifests to `package.source`, `package.main`, `package.kind`, and `[native].link`; add required struct-field comma separators.
- **Breaking:** migrate 47 binding methods to pass or return C-layout records by value. Remove single-record output parameters and input-pointer adapters across Silk declarations, Objective-C bridges, generators, and API metadata. Genuine array/buffer/error pointers are unchanged.
- Update GPU examples to use record values and verify a four-double clear-color return alongside nested-region texture readback. Consumers must rebuild/relink their native archive and executable together.

## 0.1.0

- Port the public metal-rs API at `e47bc888d3837f85ea8b01c9e66ccf73236293b9` to unsafe Silk namespaces and a scalar Objective-C bridge, without a Rust runtime dependency.
- Include Metal resources, pipelines, encoders, shader libraries, capture/synchronization, ray tracing, MPS, historical feature-set capability tables, constants, and C-layout records.
- Add RAII Objective-C ownership and autorelease pools, explicit NSError outputs, and C callback/context adapters.
- Correct SDK getter spellings and vertex-amplification record layout; adapt visible-function tables to pipeline-specific handles and the real range selector.
- Add GPU compute and headless-render examples, pinned API inventory, native layout assertions, and reproducible binding generators.
- Document explicit consumer native linking required by the current Silk compiler, raw-handle safety contracts, and private/API availability limits.
