# Overview

e3dcset is a Linux command-line tool for controlling and monitoring E3DC S10 home power plants via the RSCP protocol. The application provides real-time monitoring, battery management, historical data analysis, and dynamic tag-based configuration for E3DC energy storage systems.

# User Preferences

Preferred communication style: Simple, everyday language.

# System Architecture

## Core Architecture Pattern
- **Command-line application** built for Linux environments
- **Direct RSCP protocol communication** with E3DC S10 hardware devices over network sockets
- **Configuration-driven design** using external config files for credentials and tag definitions
- **Single-purpose execution model** where each invocation performs a specific task (query, set power, analyze history)

## Communication Layer
- **RSCP Protocol Handler**: Custom implementation for E3DC's proprietary RSCP (Remote Storage Control Protocol)
- **Network Communication**: Direct TCP/IP socket connection to E3DC device (configurable IP and port)
- **AES Encryption**: Encrypted communication channel using AES password for secure device control

## Tag Management System
- **External Tag Configuration**: Tag definitions loaded from configuration file (`e3dcset.config`)
- **Dynamic Tag Resolution**: No recompilation needed when adding new tags
- **Dual Lookup Support**: Tags searchable by name or hexadecimal value
- **Custom Interpretations**: User-defined descriptions and value interpretations per tag

## Data Access Modes

### Real-time Data Querying
- Direct tag-based queries to retrieve current device state
- Support for arbitrary RSCP tags
- Quiet mode for script integration and automation

### Battery Power Management
- Set charge/discharge power levels
- Toggle automatic vs. manual power management modes
- Start/stop manual battery charging with specific energy amounts

### Historical Data Analysis
- Time-based aggregation (day/week/month/year)
- Automatic sampling optimization based on time range
- Energy totals with self-sufficiency and self-consumption metrics
- Flexible date parsing (ISO format or relative dates like 'today')

## Configuration Management
- **Single config file approach** (`e3dcset.config`)
- Stores connection parameters (server IP, port)
- Stores authentication credentials (E3DC user, passwords)
- Includes debug flag for troubleshooting
- May contain tag definitions and custom interpretations

## Design Decisions

### Why Direct Protocol Implementation
- **Problem**: Need low-level control over E3DC devices
- **Solution**: Direct RSCP protocol implementation
- **Rationale**: Provides full access to device capabilities without middleware dependencies
- **Trade-off**: Requires maintaining protocol compatibility vs. easier high-level API usage

### Why Configuration File Over Hardcoding
- **Problem**: Credentials and device details vary per installation
- **Solution**: External configuration file
- **Rationale**: Security (credentials not in code), flexibility (no recompilation), portability
- **Trade-off**: Requires file management vs. simpler embedded defaults

### Why Command-line Over GUI
- **Problem**: Need for automation and remote administration
- **Solution**: CLI-first design with quiet mode
- **Rationale**: Script integration, remote SSH access, cron job scheduling
- **Trade-off**: Less user-friendly for non-technical users vs. easier automation

# External Dependencies

## Hardware Integration
- **E3DC S10 Home Power Plant**: Primary target hardware device
- **RSCP Protocol**: Proprietary E3DC communication protocol (port 5033 default)
- **Network Connectivity**: Local network access to E3DC device required

## Security & Authentication
- **AES Encryption**: For secure RSCP communication channel
- **E3DC User Authentication**: Email-based user credentials
- **RSCP Password**: Separate encryption password for protocol-level security

## System Requirements
- **Linux Operating System**: Target platform for execution
- **Network Socket Support**: For TCP/IP communication
- **File System Access**: For reading configuration files

## No External Web Services
- All operations are local/LAN-based
- No cloud APIs or third-party web services
- Direct device-to-device communication only