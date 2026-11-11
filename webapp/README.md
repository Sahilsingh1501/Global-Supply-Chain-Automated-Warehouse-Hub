# Northstar Supply Chain Hub

An internal warehouse and supply-chain operations website with a **native C++ backend** and a React frontend. The API uses the project's existing inventory, warehouse, route, shipment, and snapshot-persistence modules in `VER2`; Node.js is used only for frontend development and build tooling.

## Modules

- **Overview:** inventory, warehouse, shipment and purchasing metrics, movement trends, low-stock alerts and recent activity.
- **Inventory:** searchable products, per-warehouse stock, reorder levels, audited adjustments and movement history.
- **Warehouses:** facility directory, capacity usage, editable details and route connections.
- **Suppliers and purchasing:** supplier contacts, purchase orders, partial receiving, cancellations and stock intake.
- **Routes and shipments:** shipping lanes, route-checked dispatch, shipment progression, delivery, delays and event history.
- **Reports:** current stock by product and facility.
- **Team and audit:** user accounts, role-based permissions and operational audit history.

## Technology and architecture

- **Backend:** C++11 HTTP service using the original C++ inventory, warehouse and supply-chain modules. It listens on `127.0.0.1:4000` by default.
- **Persistence:** the existing `VER2` snapshot file (`warehouse_system.dat`) stores core inventory, warehouse, route and shipment data. `warehouse_web_state.json` in the same directory stores website-specific metadata and accounts.
- **Authentication:** bearer sessions expire after 12 hours; passwords are hashed with PBKDF2-SHA256 using Windows BCrypt.
- **Frontend:** React, TypeScript, Vite and Recharts. During development Vite proxies `/api` to the C++ service; in production the C++ service serves the built frontend.

The API binds to loopback only and is designed for a trusted internal/local deployment. Put it behind an authenticated reverse proxy with TLS before exposing it beyond the machine.

## Prerequisites

- Windows 10 or later
- Node.js 20 or later
- A C++ toolchain with C++11, Windows SDK headers and a working C++ threading implementation. For MSVC, install **Desktop development with C++** using Visual Studio Installer. The run file detects and initializes MSVC automatically. Alternatively, use MinGW-w64 built with the **POSIX** thread model. The older MinGW.org GCC 6.3 `win32` thread-model compiler does not provide `std::thread`/`std::mutex` and cannot build this HTTP service.

The HTTP and JSON headers are vendored under `server/vendor`; no C++ package manager is needed.

## Run in development

**Easy start:** double-click `RUN_PROJECT.bat` in the `Global-Supply-Chain-Automated-Warehouse-Hub` folder. It installs the frontend packages the first time, checks/builds the C++ API before opening the website, then starts both services in a second window. If the C++ build fails, it explains which compiler is needed instead of opening a website that cannot sign in. Keep both windows open while using the app.

You can also start it from this directory:

```powershell
npm install
npm run dev
```

`npm run dev` builds and starts the native C++ API, then starts Vite. Open <http://localhost:5173>. If you use MinGW-w64 and it is not on `PATH`, set `CXX` to the compiler executable before running the commands.

The initial local admin is `admin@example.com` / `ChangeMe123!`. Set `GSC_ADMIN_PASSWORD` (at least 10 characters) before the first launch to use a different password, and optionally set `GSC_ADMIN_EMAIL`. The demo credential is for local development only.

## Build and run production

```powershell
npm run build
npm start
```

The build type-checks and bundles the frontend, then compiles `server/main.cpp` to `server/gsc-server.exe`. `npm start` runs it from `VER2` so the existing C++ snapshot and website sidecar are both loaded and saved in the same data directory. The site is served at <http://127.0.0.1:4000>.

Configuration:

| Variable | Default | Purpose |
|---|---|---|
| `PORT` | `4000` | Local HTTP port |
| `GSC_ADMIN_EMAIL` | `admin@example.com` | Initial administrator email |
| `GSC_ADMIN_PASSWORD` | `ChangeMe123!` | Initial administrator password; minimum 10 characters |
| `GSC_WEB_ROOT` | unset | Built frontend directory; set automatically by `npm start` |

Changing admin credentials affects the initial account only. Existing account records are persisted in `warehouse_web_state.json`.

## Data and business rules

- Inventory is tracked in the existing C++ product and warehouse modules; receiving and adjustments validate stock and capacity.
- Purchase receipts cannot exceed the remaining quantity on an order line.
- Shipments require a connected route and sufficient origin stock; delivery updates destination stock and warehouse capacity.
- User roles are checked by the API, not only hidden by the frontend.
- Snapshot and web-sidecar files are created under `VER2` when the API starts. Keep backups before moving or editing these files.

## Roles

| Role | Typical access |
|---|---|
| `ADMIN` | All modules; manage team; view audit history |
| `INVENTORY` | Products, stock, warehouses, receiving and reports |
| `LOGISTICS` | Routes, shipments and reports |
| `PROCUREMENT` | Suppliers, purchase orders and reports |
| `VIEWER` | Read-only operational views |

## API overview

Operational endpoints require `Authorization: Bearer <token>` after `POST /api/auth/login`.

| Area | Endpoints |
|---|---|
| Session | `POST /api/auth/login`, `GET /api/auth/me` |
| Dashboard | `GET /api/dashboard` |
| Inventory | `GET/POST /api/products`, `PATCH /api/products/:id`, `POST /api/inventory/adjustments`, `GET /api/inventory/movements` |
| Warehouses | `GET/POST /api/warehouses`, `PATCH /api/warehouses/:id` |
| Suppliers | `GET/POST /api/suppliers`, `PATCH /api/suppliers/:id` |
| Purchasing | `GET/POST /api/purchase-orders`, `POST /api/purchase-orders/:id/receive`, `POST /api/purchase-orders/:id/cancel` |
| Routes | `GET/POST /api/routes`, `PATCH /api/routes/:id` |
| Shipments | `GET/POST /api/shipments`, `PATCH /api/shipments/:id/advance`, `PATCH /api/shipments/:id/delay` |
| Reporting | `GET /api/reports/stock` |
| Administration | `GET/POST /api/users`, `PATCH /api/users/:id`, `GET /api/audit` |
| Health | `GET /api/health` |
