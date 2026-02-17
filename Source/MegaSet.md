# MegaSet

## Bindings

- 0 -> `VK_DESCRIPTOR_TYPE_SAMPLER`       -> 256
- 1 -> `VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE` -> 16384
- 2 -> `VK_DESCRIPTOR_TYPE_STORAGE_IMAGE` -> 1024

## Flags

- Binding Flags: `VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT`
- Pool Flags:    `VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT`
- Layout Flags:  `VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT`

## Features

- Batched Descriptor Writes

## Index Allocator (Per Binding):

### Data:

- `LastAllocatedIndex` -> `u32` 
- `FreeIndices`        -> `Queue`

### Methods:

```
Allocate():
    if !FreeIndices.empty():
        index = FreeIndices.front();
        FreeIndices.pop()
    else
        index = LastAllocatedIndex
        LastAllocatedIndex = LastAllocatedIndex + 1
    
    return index
    
Free(index):
    FreeIndices.push(index)
```