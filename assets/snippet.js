const sourceCache = new Map();

function escapeHtml(text) {
    return text
        .replaceAll("&", "&amp;")
        .replaceAll("<", "&lt;")
        .replaceAll(">", "&gt;");
}

function highlightPlainCpp(text) {
    let highlighted = escapeHtml(text);

    highlighted = highlighted.replace(
        /\b(auto|bool|break|case|class|const|constexpr|continue|double|else|float|for|if|int|namespace|override|private|public|return|size_t|static|std|struct|true|false|uint32_t|virtual|void|while)\b/g,
        '<span class="code-keyword">$1</span>'
    );

    highlighted = highlighted.replace(
        /\b(Alignment|BorderMode|Camera|ColorConversion|Dimensions|FilterMode|FontEngine|FontRenderer|GLApp|GLchar|GLenum|GLfloat|GLint|GLuint|Image|Intersection|LightSource|Mat3|Mat4|Mat4t|Material|Plane|PointLight|Ray|Scene|Sphere|Tessellation|Texture|TextureCoordinates|Vec3|Vec3t|Vec4|Vec4t)\b/g,
        '<span class="code-type">$1</span>'
    );

    highlighted = highlighted.replace(
        /\b([0-9]+(\.[0-9]+)?f?)\b/g,
        '<span class="code-number">$1</span>'
    );

    return highlighted;
}

function highlightCode(code) {
    const stringPattern = /"([^"\\]|\\.)*"/g;
    let result = "";
    let lastIndex = 0;
    let match;

    while ((match = stringPattern.exec(code)) !== null) {
        result += highlightPlainCpp(code.slice(lastIndex, match.index));
        result += `<span class="code-string">${escapeHtml(match[0])}</span>`;
        lastIndex = match.index + match[0].length;
    }

    result += highlightPlainCpp(code.slice(lastIndex));
    return result;
}

function highlightCpp(line) {
    const commentStart = line.indexOf("//");
    const code = commentStart >= 0 ? line.slice(0, commentStart) : line;
    const comment = commentStart >= 0 ? line.slice(commentStart) : "";

    let highlighted = highlightCode(code);

    if (comment)
        highlighted += `<span class="code-comment">${escapeHtml(comment)}</span>`;

    return highlighted;
}

function findMarkerLine(sourceLines, marker, startIndex = 0) {
    if (!marker)
        return -1;

    if (marker === "__END_OF_FILE__")
        return sourceLines.length;

    return sourceLines.findIndex((line, index) => index >= startIndex && line.includes(marker));
}

async function loadSource(path) {
    if (!sourceCache.has(path)) {
        sourceCache.set(path, fetch(path).then(response => {
            if (!response.ok)
                throw new Error(`Could not load ${path}: ${response.status}`);

            return response.text();
        }));
    }

    return sourceCache.get(path);
}

function renderSnippet(element, sourceText) {
    const sourceLines = sourceText.split(/\r?\n/);
    const startIndex = element.dataset.start ? findMarkerLine(sourceLines, element.dataset.start) : 0;
    const endIndex = element.dataset.end ? findMarkerLine(sourceLines, element.dataset.end, startIndex + 1) : sourceLines.length;

    if (startIndex < 0 && element.dataset.start)
        throw new Error(`Could not find start marker: ${element.dataset.start}`);

    if (endIndex < 0 && element.dataset.end)
        throw new Error(`Could not find end marker: ${element.dataset.end}`);

    const snippetLines = sourceLines.slice(startIndex, endIndex).map((line, index) => ({
        line,
        lineNumber: startIndex + index + 1
    }));

    while (snippetLines.length > 0 && snippetLines[0].line.trim() === "")
        snippetLines.shift();

    while (snippetLines.length > 0 && snippetLines[snippetLines.length - 1].line.trim() === "")
        snippetLines.pop();

    const rows = snippetLines.map(({line, lineNumber}) => {
        return `<span class="code-row"><span class="line-number">${lineNumber}</span><span class="code-line">${highlightCpp(line)}</span></span>`;
    });

    element.innerHTML = `<code>${rows.join("")}</code>`;
}

async function loadSnippets() {
    const snippets = document.querySelectorAll("[data-src]");

    for (const snippet of snippets) {
        try {
            const sourceText = await loadSource(snippet.dataset.src);
            renderSnippet(snippet, sourceText);
        }
        catch (error) {
            snippet.textContent = error.message;
        }
    }
}

loadSnippets();
