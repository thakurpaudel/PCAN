# Contributing to PCAN-USB Pro FD Adapter

Thank you for considering contributing to this project! This document provides guidelines and instructions for contributing.

## Code of Conduct

- Be respectful and inclusive
- Focus on constructive feedback
- Help others learn and grow

## How to Contribute

### Reporting Bugs

1. Check if the bug has already been reported in Issues
2. Create a new issue with:
   - Clear, descriptive title
   - Steps to reproduce
   - Expected vs actual behavior
   - Hardware/software versions
   - Relevant logs or screenshots

### Suggesting Features

1. Check if the feature has been suggested
2. Create an issue describing:
   - Use case and motivation
   - Proposed implementation (if applicable)
   - Potential impact on existing functionality

### Pull Requests

1. **Fork the repository**
2. **Create a feature branch:**
   ```bash
   git checkout -b feature/your-feature-name
   ```

3. **Make your changes:**
   - Follow the coding style (see below)
   - Add comments for complex logic
   - Update documentation if needed

4. **Test your changes:**
   - Build the project successfully
   - Test on actual hardware if possible
   - Verify no regressions

5. **Commit your changes:**
   ```bash
   git commit -m "Add feature: brief description"
   ```
   
   Use clear commit messages:
   - `Add:` for new features
   - `Fix:` for bug fixes
   - `Update:` for updates to existing features
   - `Refactor:` for code refactoring
   - `Docs:` for documentation changes

6. **Push to your fork:**
   ```bash
   git push origin feature/your-feature-name
   ```

7. **Create a Pull Request:**
   - Provide a clear description
   - Reference related issues
   - List changes made
   - Include test results

## Coding Style

### C Code Style

- **Indentation**: 2 spaces (no tabs)
- **Braces**: K&R style
- **Naming**:
  - Functions: `snake_case`
  - Variables: `snake_case`
  - Constants/Macros: `UPPER_CASE`
  - Structs: `t_struct_name`

**Example:**
```c
#define MAX_BUFFER_SIZE (256)

struct t_can_message
{
  uint32_t id;
  uint8_t data[8];
};

int pcan_can_send_message(struct t_can_message *msg)
{
  if (msg == NULL)
  {
    return -1;
  }
  
  // Implementation
  return 0;
}
```

### Comments

- Use `//` for single-line comments
- Use `/* */` for multi-line comments
- Document all public functions with brief descriptions
- Explain complex algorithms

### File Organization

- Keep header guards in all `.h` files
- Group related functions together
- Separate public and private functions
- Include necessary headers only

## Development Workflow

### Setting Up Development Environment

1. **Install prerequisites** (see README.md)
2. **Clone your fork:**
   ```bash
   git clone https://github.com/your-username/PCAN.git
   cd PCAN
   ```

3. **Add upstream remote:**
   ```bash
   git remote add upstream https://github.com/original-repo/PCAN.git
   ```

4. **Create build directory:**
   ```bash
   mkdir build && cd build
   cmake ..
   ```

### Keeping Your Fork Updated

```bash
git fetch upstream
git checkout main
git merge upstream/main
git push origin main
```

## Testing

- Build in both Debug and Release modes
- Test on actual STM32H743 hardware
- Verify CAN communication on both channels
- Check USB enumeration
- Test with PCAN-compatible software

## Documentation

- Update README.md for user-facing changes
- Add inline comments for code clarity
- Update protocol documentation if applicable
- Include examples for new features

## Questions?

Feel free to open an issue for questions or clarifications.

Thank you for contributing! 🎉
