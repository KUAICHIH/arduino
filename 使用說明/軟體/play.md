# Play 功能檔案分析大綱

## 1. `PlayPage.tsx` (伺服器端組件)

### 功能概述

- 此檔案為頁面組件，負責渲染包含機器選擇功能的頁面。
- 根據 URL 參數中的 `username` 查詢用戶資料。
- 根據用戶資料渲染可選的機器列表，並檢查用戶權限。

### 關鍵步驟

1. **從 Prisma 查詢用戶資料**：
   - 使用 `prisma.user.findFirst()` 查詢用戶資料。
   - 如果找不到用戶，返回 `notFound()`。
2. **獲取當前用戶 ID**：
   - 使用 `auth()` 函數獲取當前登入用戶 ID。
   - 比對當前用戶與所選用戶，如果不一致則返回 `notFound()`。
3. **設置機器資料**：
   - 硬編碼了兩台機器資料，渲染顯示。
   - 如果需要，應改為從資料庫動態獲取。
4. **渲染機器選擇頁面**：
   - 傳遞 `machines`, `username`, `currentUserId`, `userId` 等 props 至 `MachineSelectionPage`。

### 注意事項

- 機器資料目前硬編碼，需改為動態獲取。
- 應加強用戶授權驗證。

---

## 2. `MachineSelectionPage.tsx` (前端用戶端組件)

### 功能概述

- 用戶端組件，負責顯示可選擇的機器列表，並實現機器選擇和狀態更新。
- 通過 WebSocket 實時接收機器狀態的更新，並保存選擇。

### 關鍵步驟

1. **WebSocket 連接與狀態管理**：

   - 使用 `useWebSocket` hook 管理 WebSocket 連接，實時接收機器狀態更新。
   - 當收到 WebSocket 訊息時，更新 `disabledMachines` 狀態，表示哪些機器不可用。

2. **機器選擇處理**：

   - 用戶點擊機器時，根據 `disabledMachines` 狀態檢查該機器是否可用。
   - 如果不可用，顯示錯誤訊息；如果可用，更新所選機器。

3. **保存機器選擇**：

   - 用戶確認選擇後，通過 `updateMachineCondition` API 更新選擇機器的條件。
   - 再通過 `saveMachineSelection` API 保存所選機器。
   - 顯示成功或錯誤提示。

4. **渲染機器選擇頁面**：
   - 顯示所有機器並根據 `disabledMachines` 顯示每台機器的可用狀態。
   - 當選擇機器時，將選中機器高亮顯示。

### 注意事項

- **WebSocket 管理**：
  - 必須處理 WebSocket 的連接、錯誤、重連等邏輯。
- **機器狀態管理**：

  - 使用 `disabledMachines` 狀態管理每台機器的可用性，並隨時更新顯示。

- **錯誤處理**：
  - 顯示操作過程中的錯誤訊息，並提供詳細回饋。

---

## 檔案結構

```markdown
- **PlayPage.tsx** (伺服器端組件)

  - 負責：
    - 根據 `username` 查詢用戶資料
    - 根據用戶 ID 驗證權限
    - 渲染機器選擇頁面並傳遞必要的 props 到 `MachineSelectionPage`

- **MachineSelectionPage.tsx** (用戶端組件)
  - 負責：
    - 顯示機器列表並允許用戶選擇
    - 透過 WebSocket 監控機器的實時狀態
    - 儲存用戶選擇並更新機器狀態
```
