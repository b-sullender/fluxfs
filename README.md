# **FluxFS**  
*A virtual file system and tools for creating and accessing virtual files.*  

This project is **Experimental** and currently under research, testing and development.  

## **About**  
FluxFS is a file system and file format designed to solve the issue of media servers not supporting DVD (`.ifo`) or Blu-ray playlist (`.mpls`) files. When backing up Blu-ray discs to a media server, many users prefer to retain the original structure without modifying any files. However, creating standalone MP4 or MKV files for compatibility results in unnecessary data duplication.  

FluxFS introduces a solution: **virtual files**. These files do not contain actual video, audio, or subtitle data but instead reference the original (`.VOB`) or (`.m2ts`) files. Only essential metadata is stored within the virtual file, making it a lightweight alternative to full media remuxing or conversion.  

While FluxFS was initially designed to address media server compatibility, its **virtual file format** can be applied to other use cases where file abstraction and reference-based storage are beneficial.  

For technical details, refer to the **[Specification](specification.md)**.  

## **Components**  
This repository provides:  
- **libfluxfs** – A library for creating and accessing virtual files.  
- **fluxfs** – A FUSE-based file system that presents virtual files as standard files for users and media servers.  

## **Getting Started**  
TODO...
