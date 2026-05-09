-- CreateTable
CREATE TABLE "LiveActivityToken" (
    "id" INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
    "token" TEXT NOT NULL,
    "user_id" INTEGER,
    "token_type" TEXT NOT NULL DEFAULT 'push_to_start',
    "platform" TEXT NOT NULL DEFAULT 'ios',
    "is_active" BOOLEAN NOT NULL DEFAULT true,
    "created_at" DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updated_at" DATETIME NOT NULL
);

-- CreateIndex
CREATE UNIQUE INDEX "LiveActivityToken_token_key" ON "LiveActivityToken"("token");

-- CreateIndex
CREATE INDEX "LiveActivityToken_is_active_token_type_idx" ON "LiveActivityToken"("is_active", "token_type");
