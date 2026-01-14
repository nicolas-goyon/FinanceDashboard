
import sys

# pip install psycopg2-binary
import psycopg2


# ====== HARD-CODE YOUR SETTINGS HERE ======
HOST = "localhost"
PORT = 5332
DBNAME = "mydb"
USER = "myuseradmin"
PASSWORD = "7NmssLoPfNK"

TABLE = "pizzas"   # change to a real table name
LIMIT = 50
# =========================================


def main() -> int:
    try:
        conn = psycopg2.connect(
            host=HOST,
            port=PORT,
            dbname=DBNAME,
            user=USER,
            password=PASSWORD,
        )
    except Exception as e:
        print(f"[ERROR] Could not connect: {e}", file=sys.stderr)
        return 2

    try:
        with conn, conn.cursor() as cur:
            # 1) Basic health check
            cur.execute("SELECT 1;")
            print("[OK] Connected. SELECT 1 ->", cur.fetchone()[0])

            # 2) List tables in public schema
            cur.execute(
                """
                SELECT tablename
                FROM pg_catalog.pg_tables
                WHERE schemaname = 'public'
                ORDER BY tablename;
                """
            )
            tables = [r[0] for r in cur.fetchall()]
            print("[OK] Tables in public:", tables if tables else "(none found)")

            # 3) Query a table
            # Note: identifiers (table names) can't be safely parameterized with %s,
            # so we use psycopg2.sql for safe identifier quoting.
            from psycopg2 import sql

            q = sql.SQL("SELECT * FROM {} LIMIT %s;").format(sql.Identifier(TABLE))
            # q = sql.SQL("SELECT table_name FROM information_schema.tables where table_schema = 'information_schema' LIMIT %s;")
            cur.execute(q, (LIMIT,))
            rows = cur.fetchall()

            print(f"[OK] Sample from {TABLE} (limit {LIMIT}): {len(rows)} rows")
            for row in rows:
                print(row)

        return 0

    except Exception as e:
        print(f"[ERROR] Query failed: {e}", file=sys.stderr)
        return 3

    finally:
        try:
            conn.close()
        except Exception:
            pass


if __name__ == "__main__":
    raise SystemExit(main())