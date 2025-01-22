# Developer Guide

## Overview

The Developer Guide is an essential resource for contributors to the project. It offers detailed instructions on setting up the development environment, outlines coding conventions, and provides guidelines for contributing code effectively.

## Setting Up the Development Environment

To configure your development environment, follow these steps:

1. Clone the repository:

   ```bash
   git clone https://github.com/your-username/your-repository.git
   ```

2. Navigate to the project directory:

   ```bash
   cd your-repository
   ```

3. Build the project using CMake:

   ```bash
   mkdir build
   cd build
   cmake ..
   ```

4. Compile the project:

   ```bash
   make
   ```

## Coding Conventions

Before committing any changes, it's crucial to maintain code quality and consistency. Run the **pre-commit.sh** script, which performs formatting and cppcheck checks, among other tasks. If you're unfamiliar with these tools, refer to the documentation [here](https://github.com/ItsZcx/pre-commit-hooks). Make sure Docker is installed, as the script utilizes a Docker container.


## Architecture Overview

The project follows a modular architecture, with several key classes responsible for different aspects of functionality. Below is an overview of the main classes and their responsibilities:

### 1. `class`

- asd.

These classes encapsulate the core functionalities of the project and promote modularity, scalability, and maintainability. Each class is designed to have a clear and specific responsibility, facilitating easier understanding, testing, and extension of the codebase.
