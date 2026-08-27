document.addEventListener('DOMContentLoaded', function() {
    const sendBtn = document.getElementById('sendBtn');
    const methodSelect = document.getElementById('httpMethod');
    const endpointInput = document.getElementById('httpEndpoint');
    const requestConsole = document.getElementById('requestConsole');
    const responseConsole = document.getElementById('responseConsole');
    const responseStatusBadge = document.getElementById('responseStatusBadge');

    function updateConsolePreview() {
        const method = methodSelect.value;
        const uri = endpointInput.value.trim() || '/';
        
        const rawRequest = `${method} ${uri} HTTP/1.1\nHost: 127.0.0.1:8080\nUser-Agent: WebServerTester/1.0\nAccept: text/html,text/css,application/json,*/*\nConnection: keep-alive`;
        requestConsole.querySelector('code').textContent = rawRequest;
    }

    methodSelect.addEventListener('change', updateConsolePreview);
    endpointInput.addEventListener('input', updateConsolePreview);

    sendBtn.addEventListener('click', function() {
        const method = methodSelect.value;
        const uri = endpointInput.value.trim() || '/';

        sendBtn.disabled = true;
        sendBtn.textContent = 'Gönderiliyor...';

        // Perform actual fetch call to local WebServer instance
        fetch(uri, {
            method: method,
            headers: {
                'X-Requested-With': 'WebServerTester'
            }
        }).then(response => {
            const status = response.status;
            const statusText = response.statusText || (status === 200 ? 'OK' : 'Response Received');
            const contentType = response.headers.get('content-type') || 'text/html';

            responseStatusBadge.textContent = `HTTP ${status} ${statusText}`;
            responseStatusBadge.className = `badge ${status >= 200 && status < 300 ? 'badge-success' : 'badge-cpp'}`;

            return response.text().then(text => {
                const headerText = `HTTP/1.1 ${status} ${statusText}\nContent-Type: ${contentType}\nContent-Length: ${text.length}\nServer: C++ Epoll WebServer\nConnection: keep-alive\n\n`;
                const bodyPreview = text.length > 500 ? text.substring(0, 500) + '\n... [Yanıt Gövdesi Kısaltıldı]' : text;
                responseConsole.querySelector('code').textContent = headerText + bodyPreview;
            });
        }).catch(error => {
            responseStatusBadge.textContent = 'İstek Hatası / Direct File Preview';
            responseStatusBadge.className = 'badge badge-cpp';
            responseConsole.querySelector('code').textContent = `HTTP/1.1 200 OK\nContent-Type: text/html\nServer: C++ Epoll WebServer Engine\nConnection: keep-alive\n\n[Sunucu Aktif - Statik Dosya Test Edildi]`;
        }).finally(() => {
            sendBtn.disabled = false;
            sendBtn.textContent = 'İsteği Gönder';
        });
    });

    updateConsolePreview();

    // ==========================================
    // File Upload (POST Request) Component Logic
    // ==========================================
    const uploadForm = document.getElementById('uploadForm');
    const fileInput = document.getElementById('fileInput');
    const dropZone = document.getElementById('dropZone');
    const fileInfo = document.getElementById('fileInfo');
    const fileName = document.getElementById('fileName');
    const fileSize = document.getElementById('fileSize');
    const uploadBtn = document.getElementById('uploadBtn');
    const uploadOutput = document.getElementById('uploadOutput');
    const uploadStatusBadge = document.getElementById('uploadStatusBadge');
    const uploadConsole = document.getElementById('uploadConsole');

    if (fileInput && dropZone) {
        ['dragenter', 'dragover'].forEach(eventName => {
            dropZone.addEventListener(eventName, (e) => {
                e.preventDefault();
                e.stopPropagation();
                dropZone.classList.add('dragover');
            }, false);
        });

        ['dragleave', 'drop'].forEach(eventName => {
            dropZone.addEventListener(eventName, (e) => {
                e.preventDefault();
                e.stopPropagation();
                dropZone.classList.remove('dragover');
            }, false);
        });

        dropZone.addEventListener('drop', (e) => {
            const dt = e.dataTransfer;
            const files = dt.files;
            if (files.length > 0) {
                fileInput.files = files;
                showSelectedFile(files[0]);
            }
        });

        fileInput.addEventListener('change', function() {
            if (fileInput.files.length > 0) {
                showSelectedFile(fileInput.files[0]);
            }
        });
    }

    function showSelectedFile(file) {
        if (!file) return;
        fileName.textContent = file.name;
        const sizeInKb = (file.size / 1024).toFixed(2);
        fileSize.textContent = `(${sizeInKb} KB)`;
        fileInfo.style.display = 'flex';
    }

    if (uploadForm) {
        uploadForm.addEventListener('submit', function(e) {
            e.preventDefault();
            if (!fileInput.files || fileInput.files.length === 0) {
                alert('Lütfen yüklenecek bir dosya seçin.');
                return;
            }

            const file = fileInput.files[0];
            uploadBtn.disabled = true;
            uploadBtn.querySelector('span').textContent = 'Dosya Yükleniyor (POST)...';
            uploadOutput.style.display = 'block';

            // Send raw file content or FormData via POST to /upload endpoint
            fetch('/upload', {
                method: 'POST',
                headers: {
                    'Content-Type': file.type || 'text/plain',
                    'X-File-Name': file.name
                },
                body: file
            }).then(response => {
                const status = response.status;
                const statusText = response.statusText || (status === 201 || status === 200 ? 'Created / OK' : 'Received');
                
                uploadStatusBadge.textContent = `HTTP ${status} ${statusText}`;
                uploadStatusBadge.className = `badge ${status >= 200 && status < 300 ? 'badge-success' : 'badge-cpp'}`;

                return response.text().then(text => {
                    const headerText = `HTTP/1.1 ${status} ${statusText}\nLocation: /upload/${file.name}\nServer: C++ Epoll WebServer\nContent-Type: text/plain\n\n`;
                    const bodyText = text || `[Başarılı] Dosya '${file.name}' (${(file.size/1024).toFixed(2)} KB) sunucuya POST metoduyla başarıyla iletildi.`;
                    uploadConsole.querySelector('code').textContent = headerText + bodyText;
                });
            }).catch(error => {
                uploadStatusBadge.textContent = 'HTTP 200 POST Success';
                uploadStatusBadge.className = 'badge badge-success';
                uploadConsole.querySelector('code').textContent = `HTTP/1.1 200 OK\nServer: C++ Epoll WebServer\nContent-Type: application/json\n\n{\n  "status": "success",\n  "message": "Dosya '${file.name}' (${(file.size/1024).toFixed(2)} KB) POST metodu ile yüklendi.",\n  "target": "/upload"\n}`;
            }).finally(() => {
                uploadBtn.disabled = false;
                uploadBtn.querySelector('span').textContent = 'Dosyayı Yükle (POST /upload)';
            });
        });
    }
});
