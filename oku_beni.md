# 🚀 WebServer (42 HTTP Server) — ConfigParser & ServerConfig İyileştirme ve Bug Fix Raporu

Selam! Projedeki konfigürasyon ayrıştırma (`ConfigParser`) ve sunucu konfigürasyonu (`ServerConfig`) modüllerinde tespit edilen bug'lar düzeltilmiş, kod kalitesi artırılmış ve hata yakalama mekanizmaları güçlendirilmiştir. 

Yapılan tüm değişiklikler **C++98** standartlarına tam uyumlu olup `-Wall -Wextra -Werror` bayraklarıyla sorunsuz derlenmektedir. Yapılan iyileştirmelerin detayları aşağıdadır:

---

## 🛠️ Yapılan Değişiklikler ve Detayları

### 1. `"server"` Anahtar Kelimesi İçin Sınır (Word Boundary) Kontrolü
* **Sorun:** `ConfigParser.cpp` içinde `all_conf.find("server", i)` metoduyla sunucu bloklarının başlangıcı taranıyordu. Ancak bu bir alt string (substring) araması olduğu için `someserver {` veya `my_server {` gibi metinler, `"server"` içerdiği ve devamında `{` karakterine kadar sadece boşluk bulunduğu için yanlışlıkla yeni bir `server` bloğu sanılabiliyordu.
* **Çözüm:** `"server"` kelimesi bulunduğunda, kelimeden bir önceki karakterin bağımsız bir kelime sınırı (boşluk, tab, yeni satır veya dosya başı) olup olmadığını kontrol eden `std::isspace(...)` denetimi eklendi.
* **Sonuç:** Başka bir değişken veya kelimenin parçası olan `"server"` ifadelerinin yanlışlıkla sunucu bloğu olarak algılanması engellendi.

---

### 2. Yorum Satırlarının (`#`) Üst Seviye Taramada Temizlenmesi
* **Sorun:** `ServerConfig.cpp` içindeki tokenizer yorum satırlarını atlıyordu; fakat `ConfigParser.cpp` seviyesindeki `"server"` arama mantığı yorum satırlarını dikkate almıyordu. Dosyada `# server { ... }` şeklinde pasife alınmış bir yorum satırı olduğunda, parser bunu gerçek bir blok sanıp hata fırlatıyordu.
* **Çözüm:** `ConfigParser.cpp` içerisine tırnak içi ifadeleri (`"..."` ve `'...'`) koruyarak, metindeki tüm `#` yorum satırlarını önceden temizleyen `stripComments()` fonksiyonu eklendi.
* **Sonuç:** Yorum satırlarında geçen `server` kelimeleri veya parantezler konfigürasyon parser'ını bozmaz hale getirildi.

---

### 3. Constructor Parametre İmzası İyileştirmesi (`const std::string&`)
* **Sorun:** `ServerConfig(std::string allConf)` constructor'ı parametreyi **value** (değer) olarak alıyordu. Bu da konfigürasyon bloğu her oluşturulduğunda belpekte gereksiz string kopyalanmasına (memory overhead) yol açıyordu.
* **Çözüm:** `.hpp` ve `.cpp` dosyalarındaki constructor imzası `ServerConfig(const std::string &allConf)` şeklinde sabit referans yapıldı.
* **Sonuç:** Gereksiz bellek kopyalamalarının önüne geçilerek performans artırıldı.

---

### 4. DRY (Don't Repeat Yourself) İlkesi ve Ortak `init()` Metodu
* **Sorun:** `client_max_body_size = 1024 * 1024` varsayılan değeri hem parametresiz `ServerConfig()` hem de parametreli `ServerConfig(const std::string&)` constructor'larında ayrı ayrı tekrar ediyordu (C++98'de sınıf içi varsayılan değer atama `default member initialization` desteklenmez).
* **Çözüm:** `ServerConfig` sınıfına `private` bir `init()` metodu eklendi. Tüm varsayılan değer atamaları bu metoda taşındı ve her iki constructor da ortak olarak `init()` metodunu çağıracak şekilde güncellendi.
* **Sonuç:** Kod tekrarı önlendi, kodun bakımı ve okunabilirliği kolaylaştırıldı.

---

### 5. Aynı Server Bloğu İçinde Mükerrer `listen` Kontrolü (Senaryo 1)
* **Sorun:** Bir `server` bloğu içine yanlışlıkla iki defa aynı dinleme adresi yazıldığında (örneğin üst üste iki kez `listen 127.0.0.1:8080;`), `std::set` yapısı gereği ikinci adres sessizce eziliyor ve hiçbir hata verilmiyordu.
* **Çözüm:** `ServerConfig.cpp` içindeki `applyServerDirective` fonksiyonunda `server.listens` kümesine ekleme yapılmadan önce `server.listens.find(*it)` kontrolü eklendi. Eğer adres zaten kümede varsa hemen `std::invalid_argument` hatası fırlatılıyor.
* **Sonuç:** Aynı blok içindeki mükerrer konfigürasyon hataları anında yakalanır hale getirildi.

---

### 6. Farklı Server Blokları Arasında Çakışan `listen` Kontrolü (Senaryo 2)
* **Sorun:** İki ayrı `server` bloğunda aynı `listen` adresi (örneğin her ikisinde de `listen 127.0.0.1:8080;`) kullanıldığında, dosya parse edilirken hata verilmiyordu. Sunucu başlatılırken ikinci socket `bind()` işleminde `EADDRINUSE` (adres kullanımda) hatası alıyor ve sunucu sessizce eksik socket ile çalışmaya devam ediyordu.
* **Yapılan Çözüm:** `ConfigParser.cpp` constructor'ında tüm server blokları parse edildikten hemen sonra çalışan bir doğrulama adımı eklendi. Tüm blokların `listen` adresleri global bir `std::set` üzerinde kontrol ediliyor. Çakışan bir `IP:Port` tespit edilirse, henüz socket işlemlerine başlanmadan **konfigürasyon parse aşamasında** program `std::invalid_argument` ile durduruluyor.
* **Not:** Aynı IP adresinin farklı portları kullanması (`127.0.0.1:8080` ve `127.0.0.1:8081`) tamamen geçerlidir ve desteklenmektedir.

---

### 7. Değişken ve Struct Alanlarının `lowerCamelCase` Standardına Dönüştürülmesi
* **İhtiyaç:** Proje genelindeki `lowerCamelCase` isimlendirme kuralına uyum sağlamak amacıyla `ServerConfig` ve `LocationConfig` struct alanları ile parser içi yerel değişken isimleri güncellendi.
* **Değiştirilen Struct Alanları:**
  - `ServerConfig`: `serverName`, `clientMaxBodySize`, `errorPages`
  - `LocationConfig`: `allowedMethods`, `returnCode`, `returnUrl`, `uploadEnable`, `uploadStore`, `cgiExtension`
* **Yerel Değişkenler:** `inQuote`, `portText`, `onlyWhitespace`, `resultSet`, `startPos`, `endPos`, `serverPos`, `serverBlock`, `parsedSet`, `globalListens` vb.
* **Kritik Not:** Konfigürasyon dosyasındaki (`.conf`) direktif isimleri (`client_max_body_size`, `server_name`, `allow_methods`, `upload_enable` vb.) değiştirilmemiş, sadece C++ tarafındaki değişken ve alan isimleri güncellenmiştir.

---

### 8. Enum Tanımlarının Sınıf İçi (Class-Scoped) Yapıya Taşınması ve Kapsamlandırılması
* **İhtiyaç:** Projedeki enum'ların global alanda tanımlı kalması ve yalın (bare/unscoped) şekilde kullanılması önlenerek tip güvenliği ve okunabilirlik artırıldı.
* **Yapılan Değişiklikler:**
  - `HandlerType` enum'u `EpollHandler` sınıfının `public:` alanına taşındı.
  - `State` enum'u `Socket` sınıfının `public:` alanına taşındı.
  - `StreamState` ve `ClientState` enum'ları `Client` sınıfının `public:` alanına taşındı.
  - Kod genelindeki tüm enum kullanımları `SınıfAdı::DEĞER` (örn. `EpollHandler::HANDLER_LISTEN`, `Socket::IDLE`, `Client::WAITING_FOR_REQUEST`, `RequestParser::REQUEST_LINE` vb.) şeklinde tam kapsamlı (scoped) hale getirildi.

---

### 9. `std::` Prefix Tutarlılığı ve Standart Uyum
* **İhtiyaç:** 42 kuralına uygun olarak `using namespace std;` kullanılmamakta ve tüm standart kütüphane bileşenlerine açıkça `std::` prefix'i uygulanmaktadır.
* **Yapılan Değişiklikler:**
  - `std::vector`, `std::string`, `std::map`, `std::set`, `std::pair`, `std::time_t`, `std::time`, `std::min`, `std::isspace`, `std::isdigit`, `std::isxdigit`, `std::strtoul`, `std::strtol`, `std::memset` ve `std::snprintf` kullanımlarının `std::` öneki taşıdığı tüm dosyalarda doğrulandı ve eksiksizleştirildi.

---

## 📊 Özet Değişiklik Tablosu

| Modül / Dosya | İyileştirme / Fix | Açıklama |
| :--- | :--- | :--- |
| `ConfigParser.cpp` | Sınır Kontrolü (`std::isspace`) | `"someserver"` gibi kelimelerin yanlışlıkla blok sanılması önlendi. |
| `ConfigParser.cpp` | `stripComments()` | `#` yorum satırları üst seviye taramadan önce temizlendi. |
| `ConfigParser.cpp` | Global `listen` Doğrulaması | Farklı server bloklarında çakışan aynı `IP:Port` kullanımı parse anında engellendi. |
| `ServerConfig.hpp/.cpp` | `const std::string&` | Constructor parametresindeki gereksiz string kopyalama kaldırıldı. |
| `ServerConfig.hpp/.cpp` | Private `init()` Metodu | Varsayılan ilklendirmeler tek bir değişkende toplanarak DRY ilkesine uyuldu. |
| `ServerConfig.cpp` | Blok içi `listen` Kontrolü | Aynı server bloğunda tekrar eden `listen` direktifleri engellendi. |
| Tüm Modüller | `lowerCamelCase` Refactoring | Struct alanları ve parser yerel değişkenleri `lowerCamelCase` yapıldı. |
| Tüm Modüller | Class-Scoped Enums | Tüm enum'lar ilgili sınıfın `public:` bölümüne taşındı ve `SınıfAdı::DEĞER` yapıldı. |
| Tüm Modüller | `std::` Prefix Tutarlılığı | Tüm standart kütüphane tipleri ve fonksiyonlarında `std::` prefix'i tam ve tutarlı hale getirildi. |

---

## 🎯 Derleme & Test Durumu
- **Derleme Bayrakları:** `-Wall -Wextra -Werror -std=c++98`
- **Sonuç:** 0 Warning, 0 Error. Tüm değişiklikler başarıyla derlenmekte ve test senaryolarını geçmektedir.
