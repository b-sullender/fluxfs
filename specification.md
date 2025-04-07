# Virtual File Format Specification

The virtual file format consists of the following structure:

#### 1. **File Signature**
The file begins with the NULL-terminated string **`FluxFS VF`** indicating its a FluxFS virtual file.

#### 2. **Virtual Path**
- **`uint16_t len`**: The length of the following string including the NULL character.
- **`uint8_t path[]`**: A NULL-terminated string that represents the file location on the FluxFS file system.

#### 3. **File Entries**
- **`uint8_t files`**: Specifies the number of file entries (string objects) that follow this byte.
  
#### 4. **Path String Objects**
An array of length and NULL-terminated strings.
- **`uint16_t len`**: The length of the following string including the NULL character.
- **`uint8_t path[]`**: NULL-terminated string representing a file path. Each path corresponds to an indexed entry, with the index starting at 0.

#### 5. **Type Field**
- **`uint8_t type`**: Defines the type of the following entry. The interpretation of the type field is as follows:
  - **Bit 0**: Indicates whether the entry is embedded data or reference data.
    - `0 = embedded-data`
    - `1 = reference-data`
  - **Bits 1-2**: Specifies the size of the length field:
    - `0 = uint8_t`
    - `1 = uint16_t`
    - `2 = uint32_t`
    - `3 = uint64_t`
  - **Bits 3-4**: Specifies the size of the offset field:
    - `0 = uint8_t`
    - `1 = uint16_t`
    - `2 = uint32_t`
    - `3 = uint64_t`
  - **Bits 5-7**: The index of the corresponding path string.

> **Note**: The offset field (Bits 3-4) and the index field (Bits 5-7) are only relevant when Bit 0 indicates a reference-data entry.

#### 6. **Length and Offset Fields**
- The **length field** immediately follows the type field and is sized according to the value of Bits 1-2 (length size).
- If Bit 0 is set to `1` (reference-data), an **offset field** follows the type field, with the size defined by Bits 3-4 (offset size). This offset refers to the location within the file corresponding to the index specified by Bits 5-7.
- If Bits 5-7 are all set (equal to 7) then a `uint8_t` **index field** follows the **offset field**.

#### 7. **Embedded Data**
If Bit 0 of the **type field** is set to `0` (embedded-data), then the data immediately follows the **length field**.
