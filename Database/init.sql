DROP TABLE IF EXISTS "pizzas";

CREATE TABLE "pizzas"(
    "pizza_id" INT NOT NULL PRIMARY KEY,
    "name" VARCHAR(255) NOT NULL,
    "description" VARCHAR(255) DEFAULT NULL,
    "price" DECIMAL NOT NULL,
    "vegetarian" BOOLEAN NOT NULL,
    "vegan" BOOLEAN NOT NULL
)
