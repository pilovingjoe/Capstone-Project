package com.example.lorachatapp

import androidx.room.ColumnInfo
import androidx.room.Dao
import androidx.room.Database
import androidx.room.Delete
import androidx.room.Entity
import androidx.room.Insert
import androidx.room.PrimaryKey
import androidx.room.Query
import androidx.room.Room
import androidx.room.RoomDatabase

import kotlinx.coroutines.flow.Flow

@Entity
data class DBMessage(
    @PrimaryKey(autoGenerate = true) val messageID: Int = 0,
    @ColumnInfo(name = "content") val content: String,
    @ColumnInfo(name = "sender") val sender: String,
    @ColumnInfo(name = "receiver") val receiver: String,
    @ColumnInfo(name = "date_time") val dateTime: String,
    @ColumnInfo(name = "is_me") val isMe: Boolean
)

@Dao
interface MessageDao {
    @Query("SELECT * FROM DBMessage")
    fun getAll(): List<DBMessage>

    @Query("SELECT * FROM DBMessage WHERE sender == :selectSender")
    fun loadAllBySender(selectSender: String): List<DBMessage>

    @Query("SELECT * FROM DBMessage WHERE receiver == :selectReceiver")
    fun loadAllByReceiver(selectReceiver: String): List<DBMessage>

    @Query("SELECT messageID FROM DBMessage ORDER BY messageID DESC LIMIT 1")
    fun getLastID(): Int?

    @Query("SELECT DISTINCT sender FROM DBMessage WHERE sender != 'Me' UNION SELECT DISTINCT receiver FROM DBMessage WHERE receiver != 'Me'")
    fun getUniqueContacts(): Flow<List<String>>

    @Insert
    fun insertAll(vararg dbmessage: DBMessage)

    @Delete
    fun delete(dbmessage: DBMessage)
}

@Database(entities = [DBMessage::class], version = 1)
abstract class AppDatabase : RoomDatabase() {
    abstract fun messageDao(): MessageDao

    companion object {
        @Volatile
        private var INSTANCE: AppDatabase? = null

        fun getDatabase(context: android.content.Context): AppDatabase {
            return INSTANCE ?: synchronized(this) {
                val instance = Room.databaseBuilder(
                    context.applicationContext,
                    AppDatabase::class.java,
                    "lora_chat_database"
                )
                .addCallback(object : RoomDatabase.Callback() {
                    override fun onCreate(db: androidx.sqlite.db.SupportSQLiteDatabase) {
                        super.onCreate(db)
                        // Seed data will be handled in ViewModel or Activity to use Coroutines easily
                    }
                })
                .build()
                INSTANCE = instance
                instance
            }
        }
    }
}



