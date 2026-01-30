// Custom JavaScript for PCAN documentation

document.addEventListener('DOMContentLoaded', function () {
    console.log('PCAN Documentation loaded');

    // Add copy button functionality enhancement
    const codeBlocks = document.querySelectorAll('pre code');
    codeBlocks.forEach(block => {
        block.addEventListener('copy', function () {
            console.log('Code copied to clipboard');
        });
    });
});
