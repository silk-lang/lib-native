# Migrating from the dev.14 port


The dev.17 update changes 47 binding methods to use record values rather than single-record pointers or output parameters. For example:

```silk
ComputeCommandEncoder.dispatch_threads(encoder, grid, group)
RenderPassColorAttachmentDescriptor.set_clear_color(attachment, clear)
let stored_clear = unsafe RenderPassColorAttachmentDescriptor.clear_color(attachment)
let dimensions = unsafe Device.max_threads_per_threadgroup(device)
```

Invoke the first two unsafe calls inside an `unsafe` block, or prefix each with `unsafe`. Records implement `Copy`, so passing one does not consume it. Arrays of records, raw byte buffers, NSError/reflection outputs, and optional scalar outputs still use pointers.

Update manifests from `package.root`/`source-root` to `package.source` and `package.main` (relative to `source`), from `build.artifact` to `package.kind`, and from `build.native-link-inputs` to `[native].link`. Multi-field struct declarations now require comma separators.

Rebuild the native archive and every consuming executable together: the old pointer-based C signatures are not ABI-compatible with the new value signatures. No legacy adapters are retained.
